#include "features/interaction/AutoClick.h"
void check(bool, char const*);
void autoClickTests() {
    using namespace lamium::interaction;
    using Edges = std::vector<InputEdge>;
    auto const P = InputEdge::Press, R = InputEdge::Release;
    auto ticks = [](AutoClick& click, int count) { for (int i = 0; i < count; ++i) click.tick(); };
    {
        AutoClick click;
        check(click.update().empty(), "off does nothing");
        click.configure(true, AutoMode::Periodic, 3, 1);
        check(click.update() == Edges{P}, "periodic clicks at once when switched on");
        check(click.update() == Edges{R}, "a periodic press lasts one input update");
        check(click.update().empty(), "nothing between periodic clicks");
        ticks(click, 2);
        check(click.update().empty(), "the interval counts whole ticks");
        click.tick();
        check(click.update() == Edges{P} && click.update() == Edges{R}, "the next click follows after the interval");
        ticks(click, 30);
        check(click.update() == Edges{P} && click.update() == Edges{R} && click.update().empty(),
            "a stall never produces catch-up clicks");
        click.configure(true, AutoMode::Periodic, 3, 1);
        ticks(click, 3);
        check(click.update() == Edges{P}, "configuring the same settings every tick changes nothing");
        click.update();
        click.configure(true, AutoMode::Periodic, 0, 1);
        click.tick();
        check(click.update() == Edges{P}, "the interval is at least one tick");
        click.configure(false, AutoMode::Periodic, 3, 1);
        check(click.update() == Edges{R} && click.update().empty(), "switching off releases a pending press once");
        ticks(click, 10);
        check(click.update().empty(), "off stays quiet");
    }
    {
        AutoClick click;
        click.configure(true, AutoMode::Hold, 1, 1);
        check(click.update() == Edges{P} && click.update().empty(), "hold presses once and keeps it");
        click.configure(true, AutoMode::Periodic, 5, 1);
        check(click.update() == Edges{R} && click.update() == Edges{P}, "changing to periodic releases the hold first");
        click.configure(true, AutoMode::Hold, 5, 1);
        check(click.update() == Edges{R} && click.update() == Edges{P}, "changing to hold releases the periodic press");
        click.configure(false, AutoMode::Hold, 5, 1);
        check(click.update() == Edges{R} && click.update().empty(), "switching hold off releases once");
    }
    {
        AutoClick click;
        click.configure(true, AutoMode::Hold, 1, 1);
        click.update();
        click.physical(true);
        check(click.on() && click.update().empty(), "a physical press takes priority and hold stays on");
        click.physical(false);
        check(click.update() == Edges{P}, "hold presses again after the user lets go");
        click.configure(true, AutoMode::Periodic, 2, 1);
        click.update(); click.update(); click.update();
        click.physical(true);
        ticks(click, 6);
        check(click.update().empty(), "periodic stays quiet while the user holds the button");
        click.physical(false);
        check(click.update().empty(), "no clicks saved up while held");
        ticks(click, 2);
        check(click.update() == Edges{P}, "periodic resumes after release");
    }
    {
        AutoClick click;
        auto const held = FastTrigger::WhileHeld;
        click.configure(true, AutoMode::Fast, 1, 3, held);
        click.tick();
        check(click.update().empty(), "fast click while held does nothing without the button held");
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
        click.configure(true, AutoMode::Fast, 1, 99, held);
        click.physical(true);
        click.tick();
        check(click.update().size() == 2 * AutoClick::maxClicks, "clicks per tick are bounded");
        click.configure(false, AutoMode::Fast, 1, 1, held);
        click.tick();
        check(click.update().empty(), "fast click off leaves the held button to vanilla");
    }
    {
        AutoClick click;
        click.configure(true, AutoMode::Fast, 1, 2);
        check(click.update().empty(), "fast click always waits for the next tick");
        click.tick();
        check(click.update() == Edges{P, R, P, R}, "fast click always clicks without the button held");
        check(click.update().empty(), "one burst per tick");
        click.physical(true);
        click.tick();
        check(click.update() == Edges{R, P, R, P}, "a held button keeps bursting and ends pressed");
        click.physical(false);
        click.tick();
        check(click.update() == Edges{P, R, P, R}, "releasing the button goes back to unheld bursts");
        click.configure(true, AutoMode::Fast, 1, 2, FastTrigger::WhileHeld);
        click.tick();
        check(click.update().empty(), "switching to while held stops unheld bursts");
        click.suspend();
        click.configure(true, AutoMode::Fast, 1, 2);
        click.tick();
        check(click.update().empty(), "no bursts are queued while suspended");
    }
    {
        AutoClick click;
        click.configure(true, AutoMode::Hold, 1, 1);
        click.update();
        check(click.suspend() && click.on(), "suspending owes the release of a synthetic hold and stays on");
        check(!click.suspend(), "the release is owed once");
        check(click.update() == Edges{P}, "hold presses again when input returns");
        click.configure(true, AutoMode::Periodic, 2, 1);
        click.update(); click.update(); click.update();
        click.suspend();
        ticks(click, 10);
        check(click.update().empty(), "no periodic clicks are queued while suspended");
        ticks(click, 2);
        check(click.update() == Edges{P}, "periodic resumes on schedule");
        click.configure(true, AutoMode::Fast, 1, 2, FastTrigger::WhileHeld);
        click.physical(true);
        click.forgetHeld();
        click.tick();
        check(click.update().empty(), "a forgotten held button stops fast click bursts");
    }
}
