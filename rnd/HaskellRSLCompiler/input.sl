surface
matte(float Ka = 1.0; float Kd = 1;)
{
	//float m = mod(Ka * Kd, 1), k;

	normal Nf = faceforward(normalize(N), I);
	//Oi = -Os - Os;
	//Ci = Os * Cs * (Ka * ambient() + Kd * diffuse(Nf));

	//color c = color(0, 0, 0);

	float k = float "bora" mod(Ka);

	
}
