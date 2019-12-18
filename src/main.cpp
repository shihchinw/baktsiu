#include <iostream>

#include "app.h"
#include "docopt/docopt.h"

static const char USAGE[] =
R"(Bak-Tsiu, examining every image details.

    Usage:
      baktsiu
      baktsiu [--split | --columns] <name>...
      baktsiu (-h | --help)
      baktsiu --version

    Options:
      -h --help     Show this screen.
      --version     Show version.
)";


int main(int argc, char** argv)
{
    baktsiu::App app;

    constexpr bool showHelpWhenRequest = true;
    PushRangeMarker("Parse Option");
    std::map<std::string, docopt::value> args
        = docopt::docopt(USAGE, { argv + 1, argv + argc }
        , showHelpWhenRequest, "Bak-Tsiu v" VERSION);
    PopRangeMarker();

    if (app.initialize(u8"目睭 Bak Tsiu", 1280, 720))
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
