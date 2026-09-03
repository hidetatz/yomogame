.PHONY: sound
sound:
	# bgm
	ffmpeg -y -i ./sound_raw/iwashiro_dokudoku_dog.mp3 -af       "loudnorm=I=-20:TP=-6:LRA=11" -ac 1 -ar 22050 -f s16le -acodec pcm_s16le data/tetris/iwashiro_dokudoku_dog.raw
	# sfx
	ffmpeg -y -i ./sound_raw/se_itemget_006.wav -ss 0 -t 0.4 -af "loudnorm=I=-16:TP=-3" -ac 1 -ar 22050 -f s16le -acodec pcm_s16le data/tetris/sfx_menu_move_cursor.raw
	ffmpeg -y -i ./sound_raw/8bit_kawasu_2.mp3  -ss 0 -t 0.4 -af "loudnorm=I=-16:TP=-3" -ac 1 -ar 22050 -f s16le -acodec pcm_s16le data/tetris/sfx_clear_lines_123.raw
