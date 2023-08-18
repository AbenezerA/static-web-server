# Multi-process Static HTTP Web Server

Part of Lab 6 for COMS3157 (Advanced Programming) class at Columbia University in the City of New York.

This command-line program operates a TCP/IP server and allows users to serve files from a specified root directory to HTTP clients. The server is robust against client failure and can handle multiple client requests simultaneously, preventing malicious clients from stalling the program by opening a connection and never closing it.

The program is written in C and utlilizes Socket Programming and UNIX file descriptors in addition to C-standard file pointers. To showcase its features, an example html file `index.html` is provided along with two images.

To build the program, run `make` or `make http-server`. After building, to start the server run the following command:

`./http-server <server-port> <root-directory-path>`

Where `server-port` is the port-number at which the server listens for HTTP requests and `root-directory-path` is the path (relative to root) of the directory from which the server is to serve files.

After the server is up and running, go over to a browser (or any web client), connect to the server at the specified port, and enter a valid URI to access the file at that address.

## Video Walkthrough

See below for a walkthrough of the primary implemented features:

<img src='./assets/gifwalkthrough.gif' title='Video Walkthrough' width='' alt='Video Walkthrough'/>

GIF created with [ScreenToGif](https://www.screentogif.com/) for Windows. 

## Features

Upon receipt of an HTTP request from the client at the correct port, the server extracts the URI, appends it to the root directory, and serves the file at this location. The server's response consists of a header followed by the contents of the requested file (if request is valid) or a short HTML description of an error (if request is invalid).

The server supports the GET method and accepts GET requests that are either HTTP/1.0 or HTTP/1.1. If the request does not satisfy either of these conditions, such as by sending another method or using a different HTML version, the server responds with a `501 Not Implemented` status code.

The server will respond with a `400 Bad Request` status code if the request URI does not start with a '/', contains '/../', or ends in '/..'. This type of URI is a big security risk as the client will be able to fetch arbitrary files outside of the web root. Many modern browsers automatically make these corrections.

If the server is unable to open the requested file (often due to an invalid URI), it sends a `404 Error` status code. If the request is for a directory but the URI does not end with '/', the server sends a `403 Moved Permanently` code along with the properly formatted URI and a link to the correct path within the short HTML response.

If the requested URI is valid (exists in the provided root directory), the server responds with a `200 OK` status code followed by the contents of the file at the requested URI.

## Implementation Details

The server is robust against client failure. If the client browser crashes in the middle of sending a request, the program simply closes the socket connection.

The server forks the program upon each connection with a client. Thus, multiple client requests can be handled concurrently without interference.



