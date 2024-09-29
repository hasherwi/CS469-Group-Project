# Use an official Ubuntu image as a base
FROM ubuntu:22.04

# Set environment variables to avoid user prompts during installation
ENV DEBIAN_FRONTEND=noninteractive

# Create a directory for the application
WORKDIR /app

# Copy the source files into the container
COPY server cert.pem key.pem /app

# Copy all mp3 files from the local sample-mp3s folder into the container
COPY sample-mp3s /app/sample-mp3s

# Expose the port your server will run on
EXPOSE 8080

# Run the compiled server binary
CMD ["./server"]
