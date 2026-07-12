import asyncio
import edge_tts
import miniaudio
import sys
import os

async def main():
    text = "你好主人"
    output_dir = os.path.join(os.path.dirname(os.path.dirname(__file__)), "main", "assets", "common")
    os.makedirs(output_dir, exist_ok=True)

    pcm_path = os.path.join(output_dir, "nihao_zhuren.pcm")

    communicate = edge_tts.Communicate(text, "zh-CN-XiaoxiaoNeural")
    audio_data = b""
    async for chunk in communicate.stream():
        if chunk["type"] == "audio":
            audio_data += chunk["data"]

    decoded = miniaudio.decode(audio_data, miniaudio.SampleFormat.SIGNED16, 1, 24000)

    raw = bytes(decoded.samples)
    with open(pcm_path, "wb") as f:
        f.write(raw)

    duration_ms = len(raw) / (24000 * 2) * 1000
    print(f"Generated {pcm_path}")
    print(f"  Size: {len(raw)} bytes")
    print(f"  Duration: {duration_ms:.0f}ms")
    print(f"  Sample rate: 24000Hz, 16-bit mono PCM")

if __name__ == "__main__":
    asyncio.run(main())
