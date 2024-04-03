/** LOL ! */
// LL 1
// int cx112 = 1;
int cx11 = 1; // LOL 3
int x = 1;


// ...
class Car {
    /** How fast it goes */
    int speed;

    void vroom() {
        speed = 1;
    }
}

ffc script CircularMotion
{
    // Hi
    void run(int radius, int speed, int angle, int radius2, int angle2)
    {
        printf("%d\n", cx11);
        fn();
        // hi
        auto c = new Car();
        printf("%d\n", c->speed);
    }
}


/** A lovely function. */
void fn() {

}

// ffc script CircularMotion
// {
//     void run(int radius, int speed, int angle, int radius2, int angle2)
//     {
//         if(radius2 == 0) radius2 = radius; //Circle
//         if(angle < 0) angle = Rand(360); //Random Start
//         // A lovely variable.
//         int cx = this->X;
//         int cy = this->Y;
//         while(true)
//         {
//             angle += speed;
//             if(angle < -360)angle+=360; //Wrap if below -360.
//             else if(angle > 360)angle-=360; //Wrap if above 360.
//             if(angle2==0)
//             { 
//                 this->X = cx + radius*Cos(angle);
//                 this->Y = cy + radius2*Sin(angle);
//             }
//             else //Rotate at center.
//             {
//                 this->X = cx + radius*Cos(angle)*Cos(angle2) - radius2*Sin(angle)*Sin(angle2);
//                 this->Y = cy + radius2*Sin(angle)*Cos(angle2) + radius*Cos(angle)*Sin(angle2);
//             }
//             Waitframe();
//         }
//     }
// }