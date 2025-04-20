import subprocess
import os
import time
import html
import base64
import argparse
import signal

def run_and_monitor(program, program_args=None, output_file="output.png", wait_time=1, timeout=30, html_report="report.html"):
    try:
        # Build the full command
        full_command = [program] + ([program_args] if program_args else [])
        
        print(f"Running command: {' '.join(full_command)}")
        
        # Start the program with shell=True for better compatibility
        process = subprocess.Popen(
            f"exec {" ".join(full_command)}", shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, preexec_fn=os.setsid
        )
        stdout_data, stderr_data = "", ""
        start_time = time.time()

        while time.time() - start_time < timeout:
            # wait for the process to die, if it did, stop the waiting already
            try:
                process.wait(wait_time)
                break
            except subprocess.TimeoutExpired:
                # otherwise check for the file, if it exists stop waiting
                print("Polling for file {output_file}".format(output_file=output_file))
                # Check if the file is created
                if os.path.exists(output_file):
                    # give the screenshot some time to be written properly, just in case
                    time.sleep(wait_time)
                    break
        else:
            print("Timeout reached. File not found.")
        
        # Capture output
        if process.poll() is None:
            os.killpg(os.getpgid(process.pid), signal.SIGKILL)
        stdout_data, stderr_data = process.communicate()
        
        # Read the created image file as base64
        image_base64 = ""
        if os.path.exists(output_file):
            with open(output_file, "rb") as f:
                image_base64 = base64.b64encode(f.read()).decode('utf-8')
        
        # Save results to an HTML file
        with open(os.path.join("output", html_report), "w", encoding="utf-8") as report:
            report.write("""
            <html>
            <head><title>Program Output Review</title></head>
            <body>
            <h1>Generated Image</h1>
            <img src='data:image/png;base64,{image_base64}' alt='Generated Image'/>
            <h1>Standard Output</h1>
            <pre>{stdout_data}</pre>
            <h1>Error Output</h1>
            <pre>{stderr_data}</pre>
            </body>
            </html>
            """.format(
                image_base64=image_base64,
                stdout_data=html.escape(stdout_data),
                stderr_data=html.escape(stderr_data)
            ))
        print(f"Report saved to {html_report}")
        
        if stderr_data:
            print("Error output detected:")
            print(stderr_data)
    
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Run a program, monitor a file, and capture output.")
    parser.add_argument("program", help="The program to execute.")
    parser.add_argument("directory", help="Directory containing folders to process.")
    
    args = parser.parse_args()
    
    if not os.path.isdir(args.directory):
        print("Error: Specified directory does not exist.")
        exit(1)

    folders = [f for f in os.listdir(args.directory) if os.path.isdir(os.path.join(args.directory, f))]
    
    for folder in folders:
        run_and_monitor(
            args.program,
            program_args="--screenshot {path}/images/{folder}.png --screenshot-delay 420 {folder}".format(path=os.getcwd(), folder=folder),
            output_file="{path}/images/{folder}.png".format(path=os.getcwd(), folder=folder),
            html_report="{folder}.html".format(folder=folder)
        )
