"""테스트용 가짜 GameLift 클라이언트. boto3 GameLift 클라이언트에서 쓰는 메서드만 같은 모양으로 흉내 낸다."""

import json


class FakeGameLift:
    class exceptions:  # noqa: N801 - boto3 클라이언트의 exceptions 속성과 같은 모양
        class GameSessionFullException(Exception):
            pass

        class InvalidGameSessionStatusException(Exception):
            pass

        class FleetCapacityExceededException(Exception):
            pass

    def __init__(self, searchable=(), full_sessions=(), statuses=("ACTIVE",), active_unindexed=(), activating=()):
        self.searchable = list(searchable)
        self.full_sessions = set(full_sessions)
        self.statuses = list(statuses)
        self.active_unindexed = list(active_unindexed)
        self.activating = list(activating)
        self.created = []
        self.player_sessions = []

    def search_game_sessions(self, **request):
        self.search_request = request
        return {"GameSessions": [{"GameSessionId": session_id} for session_id in self.searchable]}

    def create_player_session(self, GameSessionId, PlayerId, PlayerData):
        if GameSessionId in self.full_sessions:
            raise self.exceptions.GameSessionFullException()
        self.player_sessions.append((GameSessionId, PlayerId, json.loads(PlayerData)))
        return {"PlayerSession": {"PlayerSessionId": "psess-" + GameSessionId, "IpAddress": "127.0.0.1", "Port": 7777}}

    def create_game_session(self, **request):
        self.created.append(request)
        return {"GameSession": {"GameSessionId": "gsess-new"}}

    def describe_game_sessions(self, GameSessionId=None, **request):
        if GameSessionId is None and request.get("StatusFilter") == "ACTIVATING":
            return {"GameSessions": list(self.activating)}
        if GameSessionId is None:
            # 검색 색인에는 아직 없지만 이미 ACTIVE인 세션(가득 찬 세션, 참가 차단 세션 포함)
            return {"GameSessions": list(self.active_unindexed)}
        status = self.statuses.pop(0) if len(self.statuses) > 1 else self.statuses[0]
        return {"GameSessions": [{"GameSessionId": GameSessionId, "Status": status}]}
