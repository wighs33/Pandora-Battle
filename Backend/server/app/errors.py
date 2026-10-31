"""오류 응답 형식. 모든 오류는 {"error": 고정 코드, "message": 설명}으로 돌려주고, 클라이언트는 error 코드로 분기한다."""

from fastapi import FastAPI
from fastapi.exceptions import RequestValidationError
from fastapi.responses import JSONResponse
from starlette.exceptions import HTTPException as StarletteHTTPException


class ApiError(Exception):
    """클라이언트에 그대로 돌려줄 오류."""

    def __init__(self, status, code, message):
        super().__init__(message)
        self.status = status
        self.code = code
        self.message = message


def error_response(status, code, message):
    return JSONResponse(status_code=status, content={"error": code, "message": message})


def install_error_handlers(app: FastAPI):
    @app.exception_handler(ApiError)
    async def handle_api_error(request, error: ApiError):
        return error_response(error.status, error.code, error.message)

    @app.exception_handler(RequestValidationError)
    async def handle_invalid_request(request, error: RequestValidationError):
        # 첫 번째 문제만 알려 준다. 위치는 본문 기준 경로(players.0.kills)로 쓴다.
        problem = error.errors()[0]
        location = ".".join(str(part) for part in problem.get("loc", ()) if part != "body")
        message = "%s: %s" % (location, problem["msg"]) if location else problem["msg"]
        return error_response(400, "invalid_request", message)

    @app.exception_handler(StarletteHTTPException)
    async def handle_http_error(request, error: StarletteHTTPException):
        code = {404: "not_found", 405: "method_not_allowed"}.get(error.status_code, "http_%d" % error.status_code)
        return error_response(error.status_code, code, str(error.detail))

    @app.exception_handler(Exception)
    async def handle_unexpected_error(request, error: Exception):
        # 스택 추적은 서버(uvicorn) 로그에 남고, 클라이언트에는 내부 내용을 보이지 않는다.
        return error_response(500, "internal_error", "Internal server error.")
