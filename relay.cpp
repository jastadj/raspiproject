#include <wiringPi.h>



int main(int argc, char *argv[])
{
	

	wiringPiSetup();



	while(1)
	{

		int pin = readPin(7);
		std::cout << pin << std::cout;

	}



	return 0;

}
