#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <vector>
#include <signal.h>

#include <sys/inotify.h>
#include <sys/unistd.h>
#include <limits.h>
#include <ncurses.h>

using namespace std;

void close_program(int s){
    endwin();
    exit(0);
}

bool get_last_lines(string &path, int line_count, vector<char> &output) {
    fstream file;
    output.clear();

    int max_x, max_y;
    getmaxyx(stdscr, max_y, max_x);

    bool check_line_length = false;
    if (line_count == -1) {
        line_count = max_y;
        check_line_length = true;
    }

    try {
        file.open(path, fstream::in | fstream::ate);
    } catch (...) {
        return false;
    }

    output.push_back('\0');
    int line_length = 0;
    for (line_count++; file.tellg() > 0;) {
        file.unget();
        char c = file.peek();
        line_length++;

        if (c == '\n') {
            line_count--;
            if (check_line_length && (line_length - max_x) > 0)
                line_count -= line_length / max_x;
            line_length = 0;
        }

        if (line_count > 0) {
            output.insert(output.begin(), c);
        }
        else
            break;
    }

    file.close();
    return true;
}

void print(vector<char> &output) {
    clear();
    printw("%s", output.data());
    refresh();
}

int tail(string &path, int line_count) {
    vector<char> output;

    if (!filesystem::exists(path)) {
        cerr << "No such file or directory\n";
        return 2;
    }
    if (filesystem::is_directory(path)) {
        cerr << "Cannot open directory, must specify a file\n";
        return 3;
    }

    if (!get_last_lines(path, 0, output)) {
        cerr << "Unable to open the file \"" << path << "\"\n";
        return 4;
    }

    int fd = inotify_init();
    if (fd < 0) {
        cerr << "Unable to watch the file (inotify_init)\n";
        return 6;
    }

    int wd = inotify_add_watch(fd, path.c_str(), IN_MODIFY | IN_CREATE);
    if (wd < 0) {
        cerr << "Unable to watch the file (inotify_add_watch)\n";
        return 6;
    }

    initscr();
    noecho();
    cbreak();
    refresh();

    signal(SIGINT, close_program);

    get_last_lines(path, line_count, output);
    print(output);

    size_t struct_size = sizeof(inotify_event) + PATH_MAX + 1;
    inotify_event* event = (inotify_event*)malloc(struct_size);

    while(true) {
        if (wd < 0) {
            while ((wd = inotify_add_watch(fd, path.c_str(), IN_MODIFY | IN_CREATE)) < 0)
                sleep(1);
            event->mask = 0;
        }
        else
            read(fd, event, struct_size);

        while(!get_last_lines(path, line_count, output))
            sleep(1);
        print(output);

        if (IN_IGNORED & event->mask)
            wd = -1;
    }

    return 0;
}
  
int main(int argc, char* argv[])
{   
    string path;
    int line_count = 10;

    if (argc < 2) {
        goto wrong_arg;
    }
    path = argv[1];

    if (argc > 2) {
        try {
            line_count = stoi(argv[2]);
            if (line_count != -1 && line_count < 1)
                line_count = 1;
        } catch (...) {
            goto wrong_arg;
        }
    }

    return tail(path, line_count);

wrong_arg:
    cerr << "Usage: tailwatch FILE_PATH [NUMBER]\n";
    return 1;
}

