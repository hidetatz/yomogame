.PHONY: sound
sound:
	# bgm
	ffmpeg -y -i ./sound_raw/iwashiro_dokudoku_dog.mp3 -af       "loudnorm=I=-20:TP=-6:LRA=11" -ac 1 -ar 22050 -f s16le -acodec pcm_s16le data/tetris/iwashiro_dokudoku_dog.raw
	# sfx
	ffmpeg -y -i ./sound_raw/cursor_2.mp3    -ss 0 -t 0.2 -af "loudnorm=I=-16:TP=-3" -ac 1 -ar 22050 -f s16le -acodec pcm_s16le data/tetris/sfx_cursor.raw
	ffmpeg -y -i ./sound_raw/cursor_8.mp3    -ss 0 -t 0.2 -af "loudnorm=I=-16:TP=-3" -ac 1 -ar 22050 -f s16le -acodec pcm_s16le data/tetris/sfx_countdown.raw
	ffmpeg -y -i ./sound_raw/buchu.mp3       -ss 0 -t 0.2 -af "loudnorm=I=-16:TP=-3" -ac 1 -ar 22050 -f s16le -acodec pcm_s16le data/tetris/sfx_hard_drop.raw
	ffmpeg -y -i ./sound_raw/little_cure.mp3 -ss 0 -t 0.2 -af "loudnorm=I=-16:TP=-3" -ac 1 -ar 22050 -f s16le -acodec pcm_s16le data/tetris/sfx_hold.raw
	ffmpeg -y -i ./sound_raw/item_3.mp3      -ss 0 -t 0.2 -af "loudnorm=I=-16:TP=-3" -ac 1 -ar 22050 -f s16le -acodec pcm_s16le data/tetris/sfx_clear_lines_123.raw
	ffmpeg -y -i ./sound_raw/item_6.mp3        -ss 0 -t 0.2 -af "loudnorm=I=-16:TP=-3" -ac 1 -ar 22050 -f s16le -acodec pcm_s16le data/tetris/sfx_tetris.raw
	ffmpeg -y -i ./sound_raw/cursor_3.mp3    -ss 0 -t 0.2 -af "loudnorm=I=-16:TP=-3" -ac 1 -ar 22050 -f s16le -acodec pcm_s16le data/tetris/sfx_pause.raw
	ffmpeg -y -i ./sound_raw/decision.mp3    -ss 0 -t 0.2 -af "loudnorm=I=-16:TP=-3" -ac 1 -ar 22050 -f s16le -acodec pcm_s16le data/tetris/sfx_resume.raw
	ffmpeg -y -i ./sound_raw/cancel.mp3      -ss 0 -t 0.2 -af "loudnorm=I=-16:TP=-3" -ac 1 -ar 22050 -f s16le -acodec pcm_s16le data/tetris/sfx_cancel.raw
	ffmpeg -y -i ./sound_raw/item_8.mp3      -ss 0 -t 0.2 -af "loudnorm=I=-16:TP=-3" -ac 1 -ar 22050 -f s16le -acodec pcm_s16le data/tetris/sfx_success.raw
	ffmpeg -y -i ./sound_raw/my_down.mp3     -ss 0 -t 0.2 -af "loudnorm=I=-16:TP=-3" -ac 1 -ar 22050 -f s16le -acodec pcm_s16le data/tetris/sfx_fail.raw
