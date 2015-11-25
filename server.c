/***************************************************************************************************
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

	Setup:
	- Required libraries: curl, opencv
	- Compile and execute using: 
		- gcc -o server server.c `pkg-config --libs opencv --cflags opencv` -lcurl
		- ./server <port number>
	- Change DIR_PATH to point to your video directory

	Note:
	- audio not implemented

	References:
	http://gnosis.cx/publish/programming/sockets.html
	http://stackoverflow.com/questions/1636333/download-file-using-libcurl-in-c-c
	http://curl.haxx.se/libcurl/c/curl_easy_setopt.html
	http://docs.opencv.org/modules/highgui/doc/highgui.html
	https://learningopencv.wordpress.com/2011/05/29/example-2-2-playing-video-file/

****************************************************************************************************/

#include <stdio.h>		
#include <stdlib.h>		
#include <string.h>		
#include <arpa/inet.h>	
#include <sys/socket.h>
#include <netinet/in.h>	
#include <unistd.h>
#include <dirent.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <opencv/cv.h>
#include <opencv2/highgui/highgui_c.h>

#define MAXPENDING 5											// max connection requests
#define BUFFSIZE 1000											// max incoming string size
#define DIR_PATH "/home/wayne/code/server/videos/"				// directory path to save video files

//****************************************************************************************************
// Prototypes
void die(char *message);
void setupSocket(char *port, int *serversock, struct sockaddr_in *server);
void handleClient(int *socket);
void playVideo(char *input);
size_t write_callback(void *ptr, size_t size, size_t nmemb, FILE *stream);

//****************************************************************************************************

int main(int argc, char **argv)
{
	if(argc < 2){
		die("USAGE: server <port number>");
	}

	int serversock, clientsock;
	struct sockaddr_in server, client;

	// Create the TCP socket and bind the server to the socket
	setupSocket(argv[1], &serversock, &server);

	// Run until program is terminated
	while(1)
	{
		unsigned int clientlen = sizeof(client);
		
		// Wait for client connection
		printf("\nWaiting for client...\n");
		if((clientsock = accept(serversock, (struct sockaddr *) &client, &clientlen)) < 0)
			die("Failed to accept client connection");
		
		printf(">> Client connected: %s\n", inet_ntoa(client.sin_addr));
		// Receive string from client
		handleClient(&clientsock);
	}
}

// Prints error message and exits the program
void die(char *message)
{
	perror(message);
	exit(1);
}

// Set up and bind the server socket and begin listening for clients
void setupSocket(char *port, int *serversock, struct sockaddr_in *server){
	// Create the TCP socket
	if((*serversock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		die("Failed to create socket");
	
	// Construct the server sockaddr_in structure
	memset(server, 0, sizeof(*server));				// Clear struct
	server->sin_family = AF_INET;					// Internet/IP
	server->sin_addr.s_addr = htonl(INADDR_ANY);	// Incoming addr
	server->sin_port = htons(atoi(port));			// Server port

	// Bind the server socket
	if(bind(*serversock, (struct sockaddr *)server, sizeof(*server)) < 0)
		die("Failed to bind the server socket");
	
	// Listen on the server socket
	if(listen(*serversock, MAXPENDING) < 0)
		die("Failed to listen on server socket");

}

// Read client input string 
void handleClient(int *sock)
{
	char buffer[BUFFSIZE];
	int received = -1;

	printf("\nWaiting for input...\n");

	// Receive data from client
	if((received = recv(*sock, buffer, BUFFSIZE, 0)) < 0)
	{
		printf("Failed to receive initial bytes from client\n");
		return;
	}

	while(received > 0)
	{
		// Remove newline char from string
		if(buffer[received - 1] == '\n')
			buffer[received-1] = 0;
		else
			buffer[received] = 0;

		// If no string provided
		if(buffer[0] == 0)
			printf("Invalid url\n");
		else
			// Play video
			playVideo(buffer);

		// Check for more data from client
		printf("\nWaiting for input...\n");
		if((received = recv(*sock, buffer, BUFFSIZE, 0)) < 0)
		{
			printf("Failed to receive additional bytes from client\n");
			return;
		}
	}
	close(*sock);
}

// Parse the string and seperate the file name
// If video does not exist in memory download it from URL
// Play video full screen
void playVideo(char *input){
	char *url = input;
	char *file_name;
	DIR *dir;
	struct dirent *dirent;

	// Seperate the file name
	// File name located after the last '/' in the string
	file_name = strrchr(input,'/');
	// If no '/' found file name = url
	file_name = file_name?file_name+1:url;
	printf(">> URL: %s\n", url);
	printf(">> File: %s\n", file_name);

	// Create path to video file
	char file_path[1000] = DIR_PATH;
	strcat(file_path, file_name);

	// Open directory containing video files
	dir = opendir(DIR_PATH);
	if(!dir)
	{
		printf("Error!, Unable to open video directory.\n");
		return;
	}

	// Check if video file already exists inside directory
	while((dirent=readdir(dir)) != NULL)
	{
		// Compare existing file names to the new file
		if(!strcmp(dirent->d_name, file_name))
		{
			printf(">> %s already exists\n", file_name);
			break;
		}
	}
	closedir(dir);

	// If video does not exist in memory download it
	if(!dirent) 
	{
		FILE *fp;
		CURLcode res;

		// Initialize curl to download the video file from the url
		CURL *curl = curl_easy_init();
		printf(">> Video does not exist. Downloading...\n");
		if(curl)
		{
			fp=fopen(file_path,"wb");
			// Set up curl options
			curl_easy_setopt(curl, CURLOPT_URL, url);						// Url to work on
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,write_callback);	// Callback for writing data
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);					// Data pointer to pass to the write callback
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);				// If link redirects, follow redirection

			// Start transfer as set up by options
			res = curl_easy_perform(curl);

			// Close all connections
			curl_easy_cleanup(curl);
			fclose(fp);

			// Error occured during download
			if(res != 0)
			{
				// Transfer was unsuccessful
				printf(">> Error: %s\n", curl_easy_strerror(res));
				// Delete the incomplete file
				if(remove(file_path) < 0)
					printf(">> Failed to remove file\n");
				return;
			}
			else
				printf(">> %s downloaded successfully\n", file_name);
		}
		else
		{
			printf(">> Error!, Unable to initialize curl.\n");
			return;
		}
	}

	// Start window thread
	cvStartWindowThread();
	// Create a window
	cvNamedWindow("Video", CV_WINDOW_NORMAL);
	// Set window to fullscreen
	cvSetWindowProperty("Video", CV_WND_PROP_FULLSCREEN, CV_WINDOW_FULLSCREEN);

	// Store information about video file in capture structure
	CvCapture *capture = cvCreateFileCapture(file_path);
	if(capture == NULL)
		printf(">> Video file could not be opened.\n");
	else
	{
		IplImage* frame;
		while(1){
			// Get next frame
			frame = cvQueryFrame(capture);
			if(frame == NULL)
				break;
			// Display the frame into video window
			cvShowImage("Video", frame);
			// A new frame is displayed every 33ms
			char c = cvWaitKey(33);
			// 'ESC' key to terminate video 
			if(c==27)
				break;		
		}
	}
	// Free capture structure and close video window
	cvReleaseCapture(&capture);
	cvDestroyWindow("Video");
	return;
}

// This callback function gets called by libcurl as soon as there is data received that needs to be saved
size_t write_callback(void *ptr, size_t size, size_t nmemb, FILE *stream){
	size_t written = fwrite(ptr, size, nmemb, stream);
	return written;
}
