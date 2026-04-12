# =============================================================================
# Dockerfile for the Telegram Signal Parser service
# Listens to Telegram channel, parses scalp signals, pushes via ZMQ
#
# Build:  docker build -t telegram-parser -f services/telegram-parser/Dockerfile .
# Run:    docker run --env-file /etc/telegram-mt5-bot/.env telegram-parser
# =============================================================================

FROM python:3.11-slim

LABEL maintainer="meril" \
      component="telegram-parser" \
      description="Telegram signal listener — parses scalp signals and pushes via ZMQ PUSH"

RUN apt-get update && apt-get install -y --no-install-recommends \
        tini \
    && rm -rf /var/lib/apt/lists/*

RUN useradd --create-home --shell /bin/bash parser

WORKDIR /app

COPY services/telegram-parser/requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY services/telegram-parser/telegram_parser.py .

# Session storage (mount as volume for persistence across restarts)
RUN mkdir -p /data && chown parser:parser /data
VOLUME ["/data"]
ENV TELETHON_SESSION=/data/telegram_session

USER parser

HEALTHCHECK --interval=30s --timeout=5s --retries=3 \
    CMD python3 -c "import socket; s=socket.socket(); s.settimeout(2); s.connect(('127.0.0.1', ${ZMQ_PARSER_PORT:-5556})); s.close()" || exit 1

ENTRYPOINT ["tini", "--"]
CMD ["python3", "telegram_parser.py"]
