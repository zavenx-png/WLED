"""ZAVEN Ghost Companion: private, authenticated HTTPS voice gateway.

Device: 16kHz, mono PCM16 WAV, exactly 3s. Response: 24kHz raw PCM16 LE.
Deploy behind a valid HTTPS proxy; never place OPENAI_API_KEY in watch firmware.
"""
import hmac
import io
import os
import threading
import wave
from collections import deque

from fastapi import FastAPI, Header, HTTPException, Request
from fastapi.responses import Response
from openai import OpenAI

app = FastAPI(title="ZAVEN Ghost Voice Gateway", version="17.28")
client = OpenAI(api_key=os.environ.get("OPENAI_API_KEY"))
device_token = os.environ.get("GHOST_DEVICE_TOKEN", "")
history = deque(maxlen=8)
lock = threading.Lock()
MAX_WAV = 120000
MAX_PCM = 240000  # 5 seconds of 24kHz mono 16-bit audio


@app.get("/health")
def health():
    return {"ok": True, "service": "ghost-voice", "format": "pcm_s16le_24000"}


def _answer(wav_bytes: bytes) -> bytes:
    with io.BytesIO(wav_bytes) as stream:
        transcription = client.audio.transcriptions.create(
            model=os.getenv("GHOST_STT_MODEL", "gpt-4o-mini-transcribe"),
            file=("ghost.wav", stream, "audio/wav"),
        )
    utterance = transcription.text.strip()[:500]
    if not utterance:
        raise HTTPException(status_code=422, detail="No audible speech detected")

    with lock:
        conversation = list(history) + [{"role": "user", "content": utterance}]
        answer = client.responses.create(
            model=os.getenv("GHOST_CHAT_MODEL", "gpt-4.1-mini"),
            instructions=(
                "You are Ghost, an affectionate, witty tiny spectral companion "
                "living in a shirt-mounted device at a music festival. "
                "Respond in one short spoken sentence, max 15 words. "
                "Be playful, useful, and never claim to see or hear things you cannot."
            ),
            input=conversation,
            max_output_tokens=65,
        ).output_text.strip()
        answer = answer or "Boo! I am here with you."
        history.append({"role": "user", "content": utterance})
        history.append({"role": "assistant", "content": answer})

    audio = client.audio.speech.create(
        model=os.getenv("GHOST_TTS_MODEL", "gpt-4o-mini-tts"),
        voice=os.getenv("GHOST_VOICE", "alloy"),
        input=answer,
        response_format="pcm",
        speed=1.2,
    )
    raw = bytes(audio.content)
    if not raw:
        raise HTTPException(status_code=502, detail="Audio model returned no data")
    # Clip only on sample boundaries and taper the final 50ms to avoid a click.
    raw = raw[:MAX_PCM - (MAX_PCM % 2)]
    if len(raw) == MAX_PCM:
        b = bytearray(raw)
        n = min(1200, len(b) // 2)
        for i in range(n):
            idx = len(b) - 2 * n + 2 * i
            v = int.from_bytes(b[idx:idx + 2], "little", signed=True)
            v = int(v * (n - 1 - i) / n)
            b[idx:idx + 2] = v.to_bytes(2, "little", signed=True)
        raw = bytes(b)
    return raw


@app.post("/voice")
async def voice(request: Request, x_device_token: str = Header(default="")):
    if not device_token or not hmac.compare_digest(
        x_device_token.encode("utf-8"), device_token.encode("utf-8")
    ):
        raise HTTPException(status_code=401, detail="Device authentication failed")
    if request.headers.get("content-type", "").split(";")[0].strip() != "audio/wav":
        raise HTTPException(status_code=415, detail="Expected audio/wav")
    body = await request.body()
    if not 44 <= len(body) <= MAX_WAV:
        raise HTTPException(status_code=413, detail="Unexpected WAV length")
    try:
        with wave.open(io.BytesIO(body), "rb") as wav:
            if wav.getnchannels() != 1 or wav.getsampwidth() != 2 or wav.getframerate() != 16000:
                raise ValueError("Expected mono PCM16 16kHz")
            if not 16000 <= wav.getnframes() <= 48000:
                raise ValueError("Recording duration out of range")
    except (wave.Error, EOFError, ValueError) as ex:
        raise HTTPException(status_code=400, detail=str(ex)) from ex

    from starlette.concurrency import run_in_threadpool
    pcm = await run_in_threadpool(_answer, body)
    return Response(
        content=pcm,
        media_type="application/octet-stream",
        headers={"Cache-Control": "no-store", "X-Audio-Format": "pcm_s16le_24000"},
    )
