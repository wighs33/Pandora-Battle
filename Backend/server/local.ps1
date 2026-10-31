<#
.SYNOPSIS
    로컬 MySQL과 게임 API 서버를 준비하고 실행한다.

.DESCRIPTION
    MySQL은 설치 없이 ZIP을 푼 폴더(-MySqlHome)에서 실행한다. 설정(my.ini), 데이터(data), root 접속 정보(root.cnf)는
    그 폴더의 부모 폴더에 두고 저장소에는 넣지 않는다.

      setup    처음 한 번(다시 해도 된다): 가상 환경과 패키지, MySQL 데이터 초기화, DB(labproject, labproject_test)와 계정,
               .env, 두 DB의 마이그레이션
      start    MySQL 시작        stop    MySQL 종료        status    실행 여부
      migrate  두 DB에 새 마이그레이션 적용
      serve    API 서버 실행(http://127.0.0.1:8000, 경로 목록과 직접 호출은 /docs)
      test     단위·통합 테스트(labproject_test DB를 비우고 쓴다)
      admin    운영 명령. 나머지 인자를 python -m app.admin에 넘긴다.

    DB 계정은 둘로 나눈다. labproject_owner는 스키마를 바꾸고(마이그레이션, 테스트 초기화),
    API 서버가 쓰는 labproject_app은 행을 읽고 쓰기만 한다(SELECT, INSERT, UPDATE).
    비밀번호와 토큰 서명 키는 setup이 무작위로 만들어 .env(git 제외)에 적는다.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File Backend\server\local.ps1 setup
    powershell -ExecutionPolicy Bypass -File Backend\server\local.ps1 test
    powershell -ExecutionPolicy Bypass -File Backend\server\local.ps1 admin raid open --hp 3000000 --reward-gold 1000
#>
param(
    [Parameter(Mandatory, Position = 0)]
    [ValidateSet("setup", "start", "stop", "status", "migrate", "serve", "test", "admin")]
    [string]$Command,
    [string]$MySqlHome = $(if ($env:LABPROJECT_MYSQL_HOME) { $env:LABPROJECT_MYSQL_HOME } else { "D:\MySQL\mysql-8.4.13-winx64" }),
    [int]$Port = 3306,
    [Parameter(ValueFromRemainingArguments)]
    [string[]]$AdminArgs = @()
)

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$StateDir = Split-Path $MySqlHome -Parent
$MyIni = Join-Path $StateDir "my.ini"
$DataDir = Join-Path $StateDir "data"
$RootCnf = Join-Path $StateDir "root.cnf"
$Mysqld = Join-Path $MySqlHome "bin\mysqld.exe"
$MysqlClient = Join-Path $MySqlHome "bin\mysql.exe"
$MysqlAdmin = Join-Path $MySqlHome "bin\mysqladmin.exe"

$DotEnv = Join-Path $PSScriptRoot ".env"
$Python = Join-Path $PSScriptRoot ".venv\Scripts\python.exe"

function Invoke-Native([string]$Name, [scriptblock]$Command) {
    & $Command
    if ($LASTEXITCODE -ne 0) {
        Write-Error "$Name 실패 (종료 코드 $LASTEXITCODE)"
    }
}

function New-Secret {
    # 32바이트 난수를 URL에 그대로 쓸 수 있는 base64url 문자([A-Za-z0-9_-])로 바꾼다.
    $Bytes = New-Object byte[] 32
    [System.Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($Bytes)
    return [Convert]::ToBase64String($Bytes).TrimEnd("=").Replace("+", "-").Replace("/", "_")
}

function Import-DotEnv {
    if (-not (Test-Path $DotEnv)) {
        Write-Error ".env가 없습니다. 먼저 setup을 실행하세요."
    }
    foreach ($Line in Get-Content $DotEnv -Encoding UTF8) {
        if ($Line -match "^\s*([A-Z0-9_]+)=(.*)$") {
            [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], "Process")
        }
    }
}

function Invoke-Python([string[]]$Arguments) {
    if (-not (Test-Path $Python)) {
        Write-Error "가상 환경이 없습니다. 먼저 setup을 실행하세요."
    }
    # app 패키지와 migrations 폴더를 찾도록 이 폴더에서 실행한다.
    Push-Location $PSScriptRoot
    try {
        & $Python @Arguments
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }
    finally {
        Pop-Location
    }
}

function Test-MySqlRunning {
    # 꺼져 있으면 mysqladmin이 표준 오류에 쓴다. 이 함수 안에서는 그것을 예외로 바꾸지 않는다.
    $ErrorActionPreference = "Continue"
    # 서버가 떠 있으면 인증에 실패해도 0을 돌려준다.
    & $MysqlAdmin --host=127.0.0.1 "--port=$Port" --connect-timeout=2 --silent ping 2>$null | Out-Null
    return $LASTEXITCODE -eq 0
}

function Start-MySql {
    if (Test-MySqlRunning) {
        return
    }
    Start-Process -FilePath $Mysqld -ArgumentList "--defaults-file=`"$MyIni`"" -WindowStyle Hidden
    for ($Try = 0; $Try -lt 60; $Try++) {
        if (Test-MySqlRunning) {
            Write-Host "MySQL이 127.0.0.1:${Port}에서 실행 중입니다."
            return
        }
        Start-Sleep -Milliseconds 500
    }
    Write-Error "MySQL이 30초 안에 시작하지 않았습니다. 오류 로그: $DataDir\*.err"
}

function Invoke-RootSql([string]$Sql) {
    # 비밀번호가 명령줄에 보이지 않도록 root.cnf와 표준 입력을 쓴다.
    $Sql | & $MysqlClient "--defaults-extra-file=$RootCnf" --batch
    if ($LASTEXITCODE -ne 0) {
        Write-Error "SQL 실행 실패 (종료 코드 $LASTEXITCODE)"
    }
}

function Get-UrlPassword([string]$Name) {
    $Line = Get-Content $DotEnv -Encoding UTF8 | Where-Object { $_ -like "$Name=*" } | Select-Object -First 1
    if ($Line -notmatch "://[^:]+:([^@]+)@") {
        Write-Error ".env의 ${Name}에서 비밀번호를 읽지 못했습니다."
    }
    return $Matches[1]
}

function Invoke-Setup {
    if (-not (Test-Path $Mysqld)) {
        Write-Error "MySQL이 없습니다: $Mysqld (mysql-8.4.x-winx64.zip을 풀어 둔 폴더를 -MySqlHome으로 지정하세요)"
    }

    if (-not (Test-Path $Python)) {
        Write-Host "== 가상 환경 만들기"
        Invoke-Native "python -m venv" { python -m venv (Join-Path $PSScriptRoot ".venv") }
    }
    Write-Host "== 패키지 설치(requirements.txt)"
    Invoke-Native "pip install" { & $Python -m pip install --disable-pip-version-check -q -r (Join-Path $PSScriptRoot "requirements.txt") }

    if (-not (Test-Path $MyIni)) {
        # 이 PC에서만 접속하고(bind-address), 쓰지 않는 X Protocol 포트(33060)는 열지 않는다(mysqlx=OFF).
        @(
            "[mysqld]",
            "basedir=$($MySqlHome.Replace('\', '/'))",
            "datadir=$($DataDir.Replace('\', '/'))",
            "port=$Port",
            "bind-address=127.0.0.1",
            "mysqlx=OFF"
        ) | Set-Content -Path $MyIni -Encoding ASCII
    }

    if (-not (Test-Path $DataDir)) {
        Write-Host "== MySQL 데이터 폴더 초기화"
        Invoke-Native "mysqld --initialize-insecure" { & $Mysqld "--defaults-file=$MyIni" --initialize-insecure --console }
    }
    Start-MySql

    if (-not (Test-Path $RootCnf)) {
        # --initialize-insecure는 root 비밀번호를 비워 둔다. 바로 무작위 비밀번호로 바꾸고 root.cnf에 적는다.
        Write-Host "== root 비밀번호 설정"
        $RootPassword = New-Secret
        "ALTER USER 'root'@'localhost' IDENTIFIED BY '$RootPassword';" |
            & $MysqlClient --host=127.0.0.1 "--port=$Port" --user=root --batch
        if ($LASTEXITCODE -ne 0) {
            Write-Error "root 비밀번호를 바꾸지 못했습니다. 이미 바꾼 데이터 폴더라면 root.cnf를 직접 만드세요."
        }
        @("[client]", "user=root", "password=$RootPassword", "host=127.0.0.1", "port=$Port") |
            Set-Content -Path $RootCnf -Encoding ASCII
    }

    if (-not (Test-Path $DotEnv)) {
        $OwnerPassword = New-Secret
        $AppPassword = New-Secret
        $Server = "127.0.0.1:$Port"
        @(
            "# local.ps1 setup이 만든 로컬 개발용 설정. git에 넣지 않는다.",
            "LABPROJECT_DATABASE_URL=mysql+pymysql://labproject_app:$AppPassword@$Server/labproject",
            "LABPROJECT_OWNER_DATABASE_URL=mysql+pymysql://labproject_owner:$OwnerPassword@$Server/labproject",
            "LABPROJECT_TEST_DATABASE_URL=mysql+pymysql://labproject_app:$AppPassword@$Server/labproject_test",
            "LABPROJECT_TEST_OWNER_DATABASE_URL=mysql+pymysql://labproject_owner:$OwnerPassword@$Server/labproject_test",
            "LABPROJECT_TOKEN_SECRET=$(New-Secret)",
            "LABPROJECT_SERVER_KEY=$(New-Secret)",
            "LABPROJECT_ALLOW_DEV_LOGIN=true"
        ) | Set-Content -Path $DotEnv -Encoding UTF8
    }
    $OwnerPassword = Get-UrlPassword "LABPROJECT_OWNER_DATABASE_URL"
    $AppPassword = Get-UrlPassword "LABPROJECT_DATABASE_URL"

    Write-Host "== DB와 계정"
    $Sql = @("labproject", "labproject_test" | ForEach-Object {
        "CREATE DATABASE IF NOT EXISTS $_ CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;"
    })
    $Sql += @(
        "CREATE USER IF NOT EXISTS 'labproject_owner'@'localhost' IDENTIFIED BY '$OwnerPassword';",
        "ALTER USER 'labproject_owner'@'localhost' IDENTIFIED BY '$OwnerPassword';",
        "CREATE USER IF NOT EXISTS 'labproject_app'@'localhost' IDENTIFIED BY '$AppPassword';",
        "ALTER USER 'labproject_app'@'localhost' IDENTIFIED BY '$AppPassword';"
    )
    foreach ($Database in "labproject", "labproject_test") {
        $Sql += "GRANT ALL PRIVILEGES ON $Database.* TO 'labproject_owner'@'localhost';"
        $Sql += "GRANT SELECT, INSERT, UPDATE ON $Database.* TO 'labproject_app'@'localhost';"
    }
    Invoke-RootSql ($Sql -join "`n")

    Invoke-Migrate
    Write-Host "준비 완료. 서버: local.ps1 serve, 테스트: local.ps1 test"
}

function Invoke-Migrate {
    Import-DotEnv
    Write-Host "== 마이그레이션: labproject"
    Invoke-Python @("-m", "app.migrate")
    Write-Host "== 마이그레이션: labproject_test"
    Invoke-Python @("-m", "app.migrate", "--url-env", "TEST_OWNER_DATABASE_URL")
}

switch ($Command) {
    "setup" { Invoke-Setup }
    "start" { Start-MySql }
    "stop" {
        if (Test-MySqlRunning) {
            Invoke-Native "mysqladmin shutdown" { & $MysqlAdmin "--defaults-extra-file=$RootCnf" shutdown }
            Write-Host "MySQL을 종료했습니다."
        }
    }
    "status" {
        if (Test-MySqlRunning) { Write-Host "MySQL 실행 중 (127.0.0.1:$Port)" } else { Write-Host "MySQL 꺼짐" }
    }
    "migrate" { Start-MySql; Invoke-Migrate }
    "serve" {
        Start-MySql
        Import-DotEnv
        Invoke-Python @("-m", "uvicorn", "app.main:create_app", "--factory", "--host", "127.0.0.1", "--port", "8000")
    }
    "test" {
        Start-MySql
        Import-DotEnv
        Invoke-Python @("-m", "unittest", "discover", "-s", "tests", "-t", ".")
    }
    "admin" {
        Start-MySql
        Import-DotEnv
        Invoke-Python (@("-m", "app.admin") + $AdminArgs)
    }
}
