#include <iostream>
#include <string>
#include <vector>

#include "app.h"
#include "docopt/docopt.h"

static const char USAGE[] =
R"(Bak-Tsiu, examining every image details.

    Usage:
      baktsiu [options]
      baktsiu [options] [--split | --columns] [--] <name>...
      baktsiu (-h | --help)
      baktsiu --version

    Options:
      --width=<n>   Set window width [default: 1280]
      --height=<n>  Set window height [default: 720]
      -h --help     Show this screen.
      --version     Show version.
)";

int main(int argc, char** argv)
{
    baktsiu::App app;
    std::vector<std::string> argList = { argv + 1, argv + argc };

#ifdef __APPLE__
    // If we launch from finder, there might be a "-psn" argument represents
    // a unique process serial number. We have to remove it before we parse
    // arguments with docopt.
    for (auto iter = argList.begin(); iter != argList.end(); ++iter) {   
        if (iter->find("-psn") == 0) {
            argList.erase(iter);
            break;
        }
    }
#endif

// return 0;

    constexpr bool showHelpWhenRequest = true;
    PushRangeMarker("Parse Option");
    std::map<std::string, docopt::value> args
        = docopt::docopt(USAGE, argList, showHelpWhenRequest, "Bak-Tsiu v" VERSION);
    PopRangeMarker();

    int width = args["--width"].asLong();
    int height = args["--height"].asLong();

    if (app.initialize(u8"目睭 Bak Tsiu", width, height))
    {
        if (args["<name>"]) {
            app.importImageFiles(args["<name>"].asStringList(), true);
        }

        baktsiu::CompositeFlags composition = baktsiu::CompositeFlags::Top;
        if (args["--split"].asBool()) {
            composition = baktsiu::CompositeFlags::Split;
        } else if (args["--columns"].asBool()) {
            composition = baktsiu::CompositeFlags::SideBySide;
        }

        app.run(composition);
        app.release();
        return 0;
    }

    return 1;
}
