# Use an official Ubuntu image as a base
FROM ubuntu:latest #TODO: Switch to a specific version.

# Set environment variables to avoid user prompts during installation
ENV DEBIAN_FRONTEND=noninteractive

# Create a directory for the application
WORKDIR /app

# Copy the source files into the container
COPY server /app

# Expose the port your server will run on
EXPOSE 8080

# Run the compiled server binary
CMD ["./server"]
