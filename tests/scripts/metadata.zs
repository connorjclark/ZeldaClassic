/** COMMENT IGNORED */
// COMMENT IGNORED
int postfix = 1; // THIS COMMENT USED
/** x marks the spot */
int x = 1;

// A car, duh
class Car {
    /** How fast it goes */
    int speed;

	/** Go fast */
    void vroom() {
        speed = 1;
    }

    void vroom2() {
        vroom();
    }
}

// a ffc script called Metadata
ffc script Metadata {
    // run me
    void run(int radius, int speed, int angle, int radius2, int angle2) {
        printf("%d %d\n", postfix, 1 + x + 2);
        utils::fn(postfix);
        // i am a car
        auto c = new Car();
        printf("%d %d %d\n", c->speed, radius, utils::hmm);
    }
}

// a namespace for cool things
namespace utils {
    /** A lovely function. */
    void fn(int a) {

    }

    int hmm; // hmmmmmm
}
