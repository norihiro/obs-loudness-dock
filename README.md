# Loudness Dock plugin for OBS Studio

## Introduction

This is a plugin for OBS Studio to provide a dock window displaying EBU R 128 loudness meter.

These loudness values will be displayed.
- Momentary loudness
- Short-term loudness
- Integrated loudness
- LRA (Range of the loudness)
- True peak

## Build flow
See [main.yml](.github/workflows/main.yml) for the exact build flow.

## API

This plugin support API through obs-websocket.
See [`get_loudness.py`](example/get_loudness.py) for example.
