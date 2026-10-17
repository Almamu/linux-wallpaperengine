"""Small request/response client for the per-user wallpaper daemon."""

from __future__ import annotations

import json
import socket
from typing import Any

from . import model
from .i18n import tr


def request(command: str, **kwargs: Any) -> dict[str, Any]:
    """Send one JSON-line command and return the daemon's current status.

    RuntimeError contains a user-facing reason when the service is unavailable
    or rejects a command.
    """
    if not isinstance(command, str) or not command:
        raise ValueError(tr("Comando inválido."))
    payload = json.dumps({"command": command, **kwargs}, ensure_ascii=False).encode("utf-8") + b"\n"
    if len(payload) > 1024 * 1024:
        raise ValueError(tr("Solicitação grande demais."))
    try:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
            # KScreen detection runs inside the service and can take several
            # seconds during display changes. Do not report a false failure
            # after the command has already been accepted.
            client.settimeout(25)
            client.connect(str(model.SOCKET_FILE))
            client.sendall(payload)
            with client.makefile("rb") as stream:
                line = stream.readline(1024 * 1024 + 1)
    except (OSError, TimeoutError) as exc:
        raise RuntimeError(tr("Serviço de wallpapers indisponível. Inicie o serviço e tente novamente.")) from exc
    if not line or len(line) > 1024 * 1024 or not line.endswith(b"\n"):
        raise RuntimeError(tr("Resposta inválida do serviço de wallpapers."))
    try:
        response = json.loads(line)
    except (ValueError, UnicodeError) as exc:
        raise RuntimeError(tr("Resposta inválida do serviço de wallpapers.")) from exc
    if not isinstance(response, dict):
        raise RuntimeError(tr("Resposta inválida do serviço de wallpapers."))
    if response.get("ok") is not True:
        raise RuntimeError(str(response.get("error") or tr("O serviço recusou a solicitação.")))
    status = response.get("status")
    if not isinstance(status, dict):
        raise RuntimeError(tr("Resposta inválida do serviço de wallpapers."))
    return status
