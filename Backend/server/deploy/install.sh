#!/bin/bash
# API 서버 EC2에 새 릴리스를 설치한다. Backend/deploy.ps1이 SSM Run Command로 root 권한으로 실행한다.
#
#   install.sh <stage> <region> <배포 묶음의 s3:// 주소>
#
# 1. 배포 묶음을 새 릴리스 폴더에 풀고 가상 환경을 만든다. 지난 릴리스는 그대로 두어 되돌릴 수 있다.
# 2. SSM Parameter Store의 설정과 비밀 값으로 API 서버 환경 파일을 쓴다(root만 읽는다).
# 3. 마스터 계정으로 DB 계정을 맞추고(app.provision), 스키마 담당 계정으로 마이그레이션한다(app.migrate).
#    두 계정의 비밀번호는 이 스크립트의 프로세스에만 두고, 실행 중인 API 서버에는 넘기지 않는다.
# 4. current 링크를 새 릴리스로 바꾸고 서비스를 다시 띄운 뒤 응답을 확인한다.
set -euo pipefail

STAGE="$1"
REGION="$2"
ARTIFACT="$3"
APP_ROOT=/opt/labproject
RELEASE="$APP_ROOT/releases/$(date -u +%Y%m%dT%H%M%SZ)"
ENV_FILE=/etc/labproject/api.env
SERVICE=labproject-api
DATABASE=labproject
# RDS 인증서를 확인할 CA 묶음(AWS가 모든 리전에 쓰는 전역 묶음)
RDS_CA_URL=https://truststore.pki.rds.amazonaws.com/global/global-bundle.pem
RDS_CA_FILE="$APP_ROOT/rds-ca.pem"
HEALTH_URL=http://127.0.0.1:8000/bossraid/status
HEALTH_WAIT_SECONDS=30
KEEP_RELEASES=3

param() {
  aws ssm get-parameter --region "$REGION" --name "/labproject/$STAGE/$1" --with-decryption \
    --query Parameter.Value --output text
}

db_url() {
  # db_url <계정> <비밀번호> [DB]. 비밀번호는 deploy.ps1이 URL에 그대로 쓸 수 있는 글자(base64url)로 만든다.
  echo "mysql+pymysql://$1:$2@$DB_HOST:3306/${3:-}?ssl_ca=$RDS_CA_FILE"
}

# 설정과 비밀 값. 하나라도 읽지 못하면 여기서 멈춘다(명령 치환을 다른 명령 안에 두면 실패가 묻힌다).
DB_HOST="$(param db-endpoint)"
API_SETTINGS="$(param api-settings)"
APP_PASSWORD="$(param db-app-password)"
OWNER_PASSWORD="$(param db-owner-password)"
MASTER_PASSWORD="$(param db-master-password)"
TOKEN_SECRET="$(param token-signing-key)"
SERVER_KEY="$(param server-key)"
STEAM_WEB_API_KEY="$(param steam-web-api-key)"

# 1. 실행 계정과 Python(처음 한 번), 새 릴리스
id labproject >/dev/null 2>&1 || useradd --system --home-dir "$APP_ROOT" --shell /sbin/nologin labproject
command -v python3.12 >/dev/null || dnf install -y -q python3.12
mkdir -p "$RELEASE"
aws s3 cp --quiet --region "$REGION" "$ARTIFACT" - | tar -xz -C "$RELEASE"
python3.12 -m venv "$RELEASE/.venv"
"$RELEASE/.venv/bin/pip" install --quiet --disable-pip-version-check -r "$RELEASE/requirements.txt"
curl -fsS -o "$RDS_CA_FILE" "$RDS_CA_URL"

# 2. API 서버 환경 파일
APP_URL="$(db_url labproject_app "$APP_PASSWORD" "$DATABASE")"
(
  umask 077
  mkdir -p "$(dirname "$ENV_FILE")"
  {
    echo "LABPROJECT_STAGE=$STAGE"
    echo "LABPROJECT_DATABASE_URL=$APP_URL"
    echo "LABPROJECT_TOKEN_SECRET=$TOKEN_SECRET"
    echo "LABPROJECT_SERVER_KEY=$SERVER_KEY"
    echo "LABPROJECT_STEAM_WEB_API_KEY=$STEAM_WEB_API_KEY"
    # CloudFormation이 만든 KEY=VALUE 줄(비밀 값 없음)
    echo "$API_SETTINGS" | sed 's/^/LABPROJECT_/'
    echo "AWS_DEFAULT_REGION=$REGION"
  } > "$ENV_FILE.new"
  mv "$ENV_FILE.new" "$ENV_FILE"
)

# 3. DB 계정과 스키마
(
  cd "$RELEASE"
  export LABPROJECT_DATABASE_URL="$APP_URL"
  export LABPROJECT_OWNER_DATABASE_URL
  LABPROJECT_OWNER_DATABASE_URL="$(db_url labproject_owner "$OWNER_PASSWORD" "$DATABASE")"
  LABPROJECT_MASTER_DATABASE_URL="$(db_url admin "$MASTER_PASSWORD")" .venv/bin/python -m app.provision
  .venv/bin/python -m app.migrate
)

# 4. 새 릴리스로 바꾸고 다시 띄운다
ln -sfn "$RELEASE" "$APP_ROOT/current"
install -m 644 "$RELEASE/deploy/$SERVICE.service" "/etc/systemd/system/$SERVICE.service"
systemctl daemon-reload
systemctl enable --quiet "$SERVICE"
systemctl restart "$SERVICE"

for attempt in $(seq 1 "$HEALTH_WAIT_SECONDS"); do
  if curl -fsS -o /dev/null "$HEALTH_URL"; then
    echo "API server is up on release $(basename "$RELEASE")."
    break
  fi
  if [ "$attempt" -eq "$HEALTH_WAIT_SECONDS" ]; then
    journalctl -u "$SERVICE" -n 50 --no-pager
    exit 1
  fi
  sleep 1
done

ls -1d "$APP_ROOT"/releases/* | head -n -"$KEEP_RELEASES" | xargs -r rm -rf
