void badAnnotation()
{
    @AlwaysRunEndpoint("invalid")
    loop(int x in [0,16), 4) {

    }
}

@Authors("asd")
ffc script BadAnnotatedScript
{
	void run(){}
}
