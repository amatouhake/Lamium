#include "features/interaction/AutoClick.h"
void check(bool, char const*);
void autoClickTests() {
    using namespace lamium::interaction;
    using Edges = std::vector<InputEdge>;
    auto const P = InputEdge::Press, R = InputEdge::Release;
    auto ticks = [](AutoClick& click, int count) { for (int i = 0; i < count; ++i) click.tick(); };
    {
        AutoClick click;
        click.start(AutoMode::Periodic, 3);
        check(click.update() == Edges{P}, "periodic clicks at once when started");
        check(click.update() == Edges{R}, "a periodic press lasts one input update");
        check(click.update().empty(), "nothing between periodic clicks");
        ticks(click, 2);
        check(click.update().empty(), "the interval counts whole ticks");
        click.tick();
        check(click.update() == Edges{P} && click.update() == Edges{R}, "the next click follows after the interval");
        ticks(click, 30);
        check(click.update() == Edges{P} && click.update() == Edges{R} && click.update().empty(),
            "a stall never produces catch-up clicks");
        click.start(AutoMode::Periodic, 0);
        click.update(); click.update();
        click.tick();
        check(click.update() == Edges{P}, "the interval is at least one tick");
    }
    {
        AutoClick click;
        click.start(AutoMode::Hold);
        check(click.update() == Edges{P} && click.update().empty(), "hold presses once and keeps it");
        click.start(AutoMode::Periodic, 5);
        check(click.update() == Edges{R} && click.update() == Edges{P}, "starting periodic ends hold first");
        click.start(AutoMode::Hold);
        check(click.update() == Edges{R} && click.update() == Edges{P}, "starting hold ends periodic");
        click.stop();
        check(click.update() == Edges{R} && click.update().empty(), "stopping hold releases once");
    }
    {
        AutoClick click;
        click.start(AutoMode::Hold);
        click.update();
        click.physical(true);
        check(click.mode() == AutoMode::Off && click.update().empty(),
            "a physical press takes over hold without releasing under it");
        click.physical(false);
        ticks(click, 5);
        check(click.update().empty() && click.mode() == AutoMode::Off, "releasing the button does not resume automation");
        click.start(AutoMode::Periodic, 2);
        click.physical(true);
        ticks(click, 4);
        check(click.update().empty(), "a physical press stops periodic too");
    }
    {
        AutoClick click;
        click.setFast(true, 3);
        click.tick();
        check(click.update().empty(), "fast click does nothing without the button held");
        click.physical(true);
        check(click.update().empty(), "fast click waits for the next tick");
        click.tick();
        check(click.update() == Edges{R, P, R, P, R, P}, "each tick holding the button gives the configured clicks");
        check(click.update().empty(), "one burst per tick");
        ticks(click, 5);
        check(click.update() == Edges{R, P, R, P, R, P}, "a stall still gives one burst");
        click.tick();
        click.physical(false);
        check(click.update().empty(), "letting go ends the burst");
        click.setFast(false);
        click.physical(true);
        click.tick();
        check(click.update().empty(), "fast click off leaves the held button to vanilla");
        click.physical(false);
    }
    {
        AutoClick click;
        click.setFast(true, 2);
        click.start(AutoMode::Periodic, 4);
        click.update();
        click.physical(true);
        click.tick();
        check(click.mode() == AutoMode::Off && click.update() == Edges{R, P, R, P},
            "pressing during periodic with fast click on hands over to fast clicks");
        click.physical(false);
        click.start(AutoMode::Hold);
        click.update();
        click.cancel();
        check(!click.fast() && click.releaseOwed(), "cancel stops every mode and still owes the release");
        check(click.update() == Edges{R} && !click.releaseOwed(), "the owed release is delivered once");
        click.setFast(true, 99);
        click.physical(true);
        click.tick();
        check(click.update().size() == 2 * AutoClick::maxClicks, "clicks per tick are bounded");
    }
}
