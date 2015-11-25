# Remote_Linux_video_player
https://www.youtube.com/watch?v=uw1ZF-nxOGY
	
	How it works:
		1.	Create TCP server on given port
		2.	Wait for client connection
		3.	Wait for client input (ex. examplelink.com/videofile.avi)
		4.	Parse the input string and get file name
		5.	Open video directory and search for the file
			5b. If file doesn't exist download the file
		6.	Play video full screen
		7.	If client disconnects go to step 2 else go to step 3
