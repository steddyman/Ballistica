// routine to check if brick above the one passed, brickno returned
int above(int n)
{
	int t;

	t=n-BricksX;
        if(t<0) return -1;       // if past top, non above
        if(bricks[t].deleted()) return -1;
        if(bricks[t].moving()) return -1; // no hit if deleted or moving
        return t;       // else there is a brick above
}

// routine to check brick below current one
int below(int n)
{
	int t;

	t=n+BricksX;
        if(t>NumBricks-1) return -1;     // if below bottom, no brick
        if(bricks[t].deleted()) return -1;
        if(bricks[t].moving()) return -1; // no hit if deleted or moving
        return t;       // brick there
}

// check for brick to right
int toright(int n)
{
	int t;

	t=n+1;
        if(n%BricksX == BricksX-1) return -1; // past right so no brick
        if(bricks[t].deleted()) return -1;
        if(bricks[t].moving()) return -1; // no hit if deleted or moving
        return t;       // brick there
}

// check for brick to left
int toleft(int n)
{
	int t;

	t=n-1;
        if(n%BricksX == 0) return -1; // past left so no brick
        if(bricks[t].deleted()) return -1;
        if(bricks[t].moving()) return -1; // no hit if deleted or moving
        return t;       // brick there
}
