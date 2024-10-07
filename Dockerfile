# Use an official Ubuntu image as a base
FROM ubuntu:22.04

# Set environment variables to avoid user prompts during installation
ENV DEBIAN_FRONTEND=noninteractive

# Create a non-root user and group
RUN groupadd -r appgroup && useradd -r -g appgroup appuser

# Create a directory for the application
WORKDIR /app

# Copy the source files into the container
COPY server cert.pem key.pem /app
COPY sample-mp3s /app/sample-mp3s

# Change ownership of copied files.
RUN chown -R appuser:appgroup /app

# Switch to the non-root user
USER appuser

# Expose the port your server will run on
EXPOSE 8080

# Run the compiled server binary
CMD ["./server"]
