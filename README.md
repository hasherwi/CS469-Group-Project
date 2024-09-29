# CS469, Group 1, Project

## Proposal

Members: Corey Brantley, Shen Knoll, Harrison Sherwin

"Build a media server that stores MP3 audio files that can be downloaded and played by a client. The server process should be replicated to provide fault tolerance, but the data does not need to be for this project. The client should be able to request a listing of available audio files then choose one to download. The client must be authenticated using cryptographic methods."

The server code will be packaged as a Docker container image to implement virtualization and require less installation for the user. Containerization will allow for easier access to fault tolerance as a multiple servers can run on one or more nodes with the help of a container orchestration system like Kubernetes (K8s). Further, the client code will not quit when playback ceases; nor will the server quit after a single file is downloaded. Both will only quit on a fatal error or by user direction.  SSL over TCP will be used to provide some authentication and security between client and server. A Kubernetes Service will be used to distribute traffic across different server containers to achieve transparency. The client will need to accomodate for this by establishing TCP over SSL sessions with each call. The complete package will be provided as a Kubernetes manifest file for ease of server setup.Hashes will be available server-side of each available MP3 file to allow for data validation by clients.

Clients will implement a retry algorithm if downloads are corrupt or failed. Clients will use the libmpg123 and libao C libraries to decode and playback MP3 files. Clients will cache the current MP3 file.

Servers will store MP3 files at this time. An obvious improvement would be storing MP3 files inside something like Amazon S3 and having the client utilize the AWS S3 SDK; however, the would circumvent the assignement. Server containers will run with a heightened, non-default Kubernetes security context to prevent an attacker from escalating privledges.  The server will expect at least 3 container instances to achieve replication. Consistency can be maintained with the use of a Kubernetes maximum pod lifetime and update strategy.

Servers will support the following actions from the client:
1) List all MP3 files available.
2) List all MP3 files available matching a search term.
3) Download a MP3 file.
4) Validate a MP3 file (download it's hash).

Clients will support the following actions from the user:
1) List MP3 files available.
2) Search for MP3 files.
3) Play a MP3 file.

Source for inspiration for playing back MP3 files: https://hzqtc.github.io/2012/05/play-mp3-with-libmpg123-and-libao.html

High Level Architecture Diagram:
Client > K8s Service (Load Balancer Type)  > Replicated Server Containers

The K8s service and containers will run on nodes of the Kubernetes cluster. Kubernetes cluster architecture is explained here: https://kubernetes.io/docs/concepts/architecture/

UML Diagram:
/TODO/

## File Descriptions
server.c - Server code in C language.\
client.c - Client code in C language.\
playaudio.c - A component of the client code in C language.\
Makefile - Used to compile C code above.\
Dockerfile - Used to containerize the server code.\
k8s-manifest.yml - Used to describe how to run the server container with Kubernetes.\
sample-mp3s/ - Contains MP3 files for the server to hold.

## How to Setup Dev Environment

/TODO/

## How to Run Server

The server is available here as C source code. In the future, we'll provide an executable, Docker image, and Kubernetes manifest.

## How to Run Client

The client is available here as C source code. In the future, we'll provide an executable.
