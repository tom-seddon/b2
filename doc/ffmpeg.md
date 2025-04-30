# FFmpeg notes

Get FFmpeg here: https://www.ffmpeg.org

Here are some almost-copy-and-pasteable examples that you can use with
the uncompressed TGAs that b2 saves out.

`PREFIX` in each case refers to the prefix used to save the files.
Replace it with the actual prefix you used.

## Basic conversion

Convert TGAs and WAVs to mp4 with flac audio, adjusting resolution to
match the BBC's slightly thin pixels. The output resolution will be
around 706x576.

    ffmpeg -y -f concat -i PREFIX.video_list.txt -f concat -i PREFIX.audio_list.txt -map 0:v:0 -map 1:a:0 -vf "scale=iw*0.96:ih" -acodec flac output.mp4
	
## 720p output
	
As above, but stretc0 the resulth to produce 720p output (1280x720)
with borders left and right.

    ffmpeg -y -f concat -i PREFIX.video_list.txt -f concat -i PREFIX.audio_list.txt -map 0:v:0 -map 1:a:0 -vf "scale=iw*0.96:ih,scale=1280:720:force_original_aspect_ratio=1,pad=width=1280:height=720:x=(out_w-in_w)/2:y=(out_h-in_h)/2:color=black" -acodec flac output.mp4
	
## 1080p output
	
And again, but this time for 1080p (1920x1080), similarly.

    ffmpeg -y -f concat -i PREFIX.video_list.txt -f concat -i PREFIX.audio_list.txt -map 0:v:0 -map 1:a:0 -vf "scale=iw*0.96:ih,scale=1920:1080:force_original_aspect_ratio=1,pad=width=1920:height=1080:x=(out_w-in_w)/2:y=(out_h-in_h)/2:color=black" -acodec flac output.mp4
