# Use a base image with a C compiler
FROM gcc:latest #TODO: Switch to a specific version.

# Set the working directory inside the container
WORKDIR /app

# Copy the source code to the container
COPY server.c /app

# Compile the C code
RUN gcc -o server *.c

# Copy the MP3 files to the container
COPY music/*.mp3 /app/music

# Set the entrypoint to run the compiled program
CMD ["./server"]
