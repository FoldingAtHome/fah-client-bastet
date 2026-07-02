// Drives FAH::Client::Unit's DB-loaded constructor and reports key
// invariants of the rehydrated Unit to stdout for the test harness to
// diff.  Read a saved-unit JSON blob from stdin; one blob per run.

#include <fah/client/App.h>
#include <fah/client/Unit.h>

#include <cbang/ApplicationMain.h>
#include <cbang/SmartPointer.h>
#include <cbang/json/Reader.h>

#include <iostream>


namespace FAH {
  namespace Client {
    class UnitLoadTest : public App {
    public:
      void run() override {
        setup();

        auto blob = cb::JSON::Reader(std::cin).parse();
        cb::SmartPointer<Unit> u = new Unit(*this, blob);

        std::cout << "state="    << u->getState()             << '\n';
        std::cout << "id="       << u->getID()                << '\n';
        std::cout << "wu="       << u->getU64("number")       << '\n';
        std::cout << "group="    << u->getString("group")     << '\n';
        std::cout << "cpus="     << u->getU32("cpus")         << '\n';
        std::cout << "run_time=" << u->getU64("run_time", 0)  << '\n';
      }
    };
  }
}


int main(int argc, char *argv[]) {
  return cb::doApplication<FAH::Client::UnitLoadTest>(argc, argv);
}
