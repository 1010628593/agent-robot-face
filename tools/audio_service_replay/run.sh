#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
cc -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined \
  -I tools/audio_service_replay/include \
  -I firmware/components/bot_audio/include \
  -I firmware/components/bot_audio_core/include \
  firmware/components/bot_audio/bot_audio.c \
  firmware/components/bot_audio_core/audio_features.c \
  firmware/components/bot_audio_core/audio_detector.c \
  tools/audio_service_replay/replay.c -lm -o /tmp/bot-audio-service-replay
/tmp/bot-audio-service-replay
