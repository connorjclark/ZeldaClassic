/** COMMENT IGNORED */
// COMMENT IGNORED
int postfix = 1; // THIS COMMENT USED
/** x marks the spot
 * still line one still line one still line one still line one still line one
 * 
 * line two
 * line two still
 *
 * line three
 *
 * - list item 1
 * - list item 2
 * - list item 3
*/
int x = 1;
// hello world
int y, z = 2;

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

// how fancy something looks
enum Brand {
    // top class
    Fancy,
    // absolute trash
    Dull,
    Mid // meh
};

// a ffc script called Metadata
ffc script Metadata {
    // run me
    void run(int radius, int speed, int angle, int radius2, int angle2) {
        printf("%d %d\n", postfix, 1 + x + 2);
        utils::fn(postfix);
        // i am a car
        auto c = new Car();
        printf("%d %d %d\n", c->speed, radius, utils::hmm + y + z + Dull + Fancy + Mid);
    }
}

// a namespace for cool things
namespace utils {
    /** A lovely function. */
    void fn(int a) {

    }

    int hmm; // hmmmmmm
}
