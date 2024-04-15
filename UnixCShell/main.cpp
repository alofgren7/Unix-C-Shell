/*
 * Copyright (c) 2022, Justin Bradley
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
 
#include "command.hpp"
#include "parser.hpp"

#define MAX_ALLOWED_LINES 25
bool skip_next_command = false;
bool previous_success = true;
void execute_commands(const std::vector<shell_command>& shell_commands);

int main(int argc, char* argv[])
{
    std::string input_line;

    if (argc > 1 && std::string(argv[1]) == "-t") {
        // Testing mode
        while (std::getline(std::cin, input_line)) {
            if (input_line == "exit") {
                break;
            }

            try {
                std::vector<shell_command> shell_commands = parse_command_string(input_line);
                execute_commands(shell_commands);  // This function encapsulates the command execution logic
            } catch (const std::exception& e) {
                std::cerr << e.what() << '\n';
            }
        }
    } else {
        // Normal shell mode
        for (int i=0;i<MAX_ALLOWED_LINES;i++) {
            std::cout << "osh> " << std::flush;

            if (!std::getline(std::cin, input_line) || input_line == "exit") {
                break;
            }

            try {
                std::vector<shell_command> shell_commands = parse_command_string(input_line);
                execute_commands(shell_commands);  // This function encapsulates the command execution logic
            } catch (const std::exception& e) {
                std::cerr << "osh: " << e.what() << '\n';
            }
        }
    }

    if (argc <= 1 || std::string(argv[1]) != "-t") {
        std::cout << std::endl;
    }
}

void execute_commands(const std::vector<shell_command>& shell_commands) {    
    next_command_mode previousMode = next_command_mode::always;
    int pipeIn = 0;
    int pipeOut = 0;
    int status = 0;
    int child_status = 0;

    for(shell_command command: shell_commands) 
    {
        // If a condition is met when it shouldn't be, break the current command chain.
        if(child_status != 0 && previousMode == next_command_mode::on_success)
        {
            break;
        }
        if(child_status == 0 && previousMode == next_command_mode::on_fail)
        {
            break;
        }

        int pipes[2];
        if(pipe(pipes) == -1)
        {
            std::cerr << "Failed\n";
            exit(1);
        }

        int pid;
        pid = fork();
        if (pid < 0) {
            std::cerr << "Fork Failed\n";
            exit(1);
        }

        else if(pid == 0) // child process
        {            
            // Convert vector<string> to vector<char*> for execvp
            std::vector<char*> cargs;
            cargs.push_back(const_cast<char*>(command.cmd.c_str()));
            for (const auto& arg : command.args) {
                cargs.push_back(const_cast<char*>(arg.c_str()));
            }
            cargs.push_back(NULL);

            // Redirect stdout
            if(command.cout_mode == ostream_mode::append){
                int fd = open(command.cout_file.c_str(), O_APPEND | O_WRONLY);
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            // Redirect stdout to the file
            if(command.cout_mode == ostream_mode::file) {
                int fd = open(command.cout_file.c_str(), O_CREAT | O_RDWR, 0666);
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            // Redirect stdin to the command
            if(command.cin_mode == istream_mode::file) {
                int fd = open(command.cin_file.c_str(), O_RDONLY );
                dup2(fd, STDIN_FILENO);
                close(fd);
            }

            // Piping Input
            if(command.cin_mode == istream_mode::pipe){
                dup2(pipeIn,STDIN_FILENO);
                close(pipes[0]);
            }

            // Piping Output
            if(command.cout_mode == ostream_mode::pipe){
                dup2(pipes[1],STDOUT_FILENO);
                close(pipes[1]);
            }
            
            execvp(cargs[0], &cargs[0]);

            // If execvp returns, there was an error
            std::cerr << "osh: Command not found\n";

            exit(1);
        }
        
        else { // Parent process
            wait(&status); // Wait for the child to finish
            pipeIn = pipes[0];
            pipeOut = pipes[1];


            // If there's an and, return the resulting status
            if(command.next_mode == next_command_mode::on_success) 
            {   
                previousMode = command.next_mode;
                child_status = status;
            }

            // If there's an or, return the resulting status
            if(command.next_mode == next_command_mode::on_fail) {
                previousMode = command.next_mode;
                child_status = status;
            }


            if(command.cout_mode != ostream_mode::term){
                dup2(STDOUT_FILENO,pipeOut);
            }

        }
    }
} 
