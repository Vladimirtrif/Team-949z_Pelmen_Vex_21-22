#include "main.h"
#include "pros/adi.h"
#include "pros/api_legacy.h"
/*
 * Presence of these two variables here replaces _pros_ld_timestamp step in common.mk.
 * THis way we get equivalent behavior without extra .c file to compile, and this faster build.
 * Pros uses value from _PROS_COMPILE_TIMESTAMP to show time on cortex / remote (I think).
 * I'm not sure how _PROS_COMPILE_DIRECTORY is used, but it's not full path (only first 23 characters)
 * and not clear if it matters at all (likely also shows somewhere on cortex, which has no usage).
 * If both of these variables are not present, final binary output becomes larger. Not sure why.
 */
extern "C" char const *const _PROS_COMPILE_TIMESTAMP = __DATE__ " " __TIME__;
extern "C" char const *const _PROS_COMPILE_DIRECTORY = "";

/**
 * A callback function for LLEMU's center button.
 *
 * When this callback is fired, it will toggle line 2 of the LCD text between
 * "I was pressed!" and nothing.
 */
/*
void on_center_button()
{
	static bool pressed = false;
	pressed = !pressed;
	if (pressed)
	{
		pros::lcd::set_text(2, "I was pressed!");
	}
	else
	{
		pros::lcd::clear_line(2);
	}
}
*/
/**
 * Runs initialization code. This occurs as soon as the program is started.
 *
 * All other competition modes are blocked by initialize; it is recommended
 * to keep execution time for this mode under a few seconds.
 */
void initialize()
{
	/*
	pros::lcd::initialize();
	pros::lcd::set_text(1, "Hello PROS User!");

	pros::lcd::register_btn1_cb(on_center_button);
	*/
}

/**
 * Runs while the robot is in the disabled state of Field Management System or
 * the VEX Competition Switch, following either autonomous or opcontrol. When
 * the robot is enabled, this task will exit.
 */
void disabled() {}

/**
 * Runs after initialize(), and before autonomous when connected to the Field
 * Management System or the VEX Competition Switch. This is intended for
 * competition-specific initialization routines, such as an autonomous selector
 * on the LCD.
 *
 * This task will exit when the robot is enabled and autonomous or opcontrol
 * starts.
 */
void competition_initialize() {}

class Autonomous
{
	pros::Controller master{CONTROLLER_MASTER};

	pros::Motor left_front{LeftFrontPort};
	pros::Motor left_middle{LeftMiddlePort, true};
	pros::Motor left_back{LeftBackPort};

	pros::Motor right_front{RightFrontPort, true};
	pros::Motor right_middle{RightMiddlePort};
	pros::Motor right_back{RightBackPort};

	pros::Motor lift_Front{frontLift, MOTOR_GEARSET_36, true}; // Pick correct gearset (36 is red)
	pros::Motor lift_Back{backLift, MOTOR_GEARSET_36, true};

	void Move(int ticks, int speed)
	{
		left_front.move_relative(ticks, speed);
		left_middle.move_relative(ticks, speed);
		left_back.move_relative(ticks, speed);

		right_front.move_relative(ticks, speed);
		right_middle.move_relative(ticks, speed);
		right_back.move_relative(-ticks, speed);
	}

	void Turn(int degrees)
	{ //code tp turn, find value to plug in instead or 1 for amount of ticks it takes to turn 360 degrees, positive is right turn, negative degrees is left turn
		left_front.move_relative((degrees / 360) * 1, 200);
		left_middle.move_relative((degrees / 360) * 1, 200);
		left_back.move_relative((degrees / 360) * 1, 200);

		right_front.move_relative((degrees / 360) * -1, 200);
		right_middle.move_relative((degrees / 360) * -1, 200);
		right_back.move_relative((degrees / 360) * 1, 200);
	}

public:
	void run()
	{
		lift_Front.set_brake_mode(pros::E_MOTOR_BRAKE_HOLD);

		//moving forward to goal
		Move(2700, 200);

		lift_Front.move_relative(-1800, 80);

		pros::delay(1350);

		lift_Front.move_relative(1000, 100);
		pros::delay(400);

		Move(-2800, 200);
	}
};

/**
 * Runs the user autonomous code. This function will be started in its own task
 * with the default priority and stack size whenever the robot is enabled via
 * the Field Management System or the VEX Competition Switch in the autonomous
 * mode. Alternatively, this function may be called in initialize or opcontrol
 * for non-competition testing purposes.
 *
 * If the robot is disabled or communications is lost, the autonomous task
 * will be stopped. Re-enabling the robot will restart the task, not re-start it
 * from where it left off.
 */
void autonomous()
{
	Autonomous self_drive;
	self_drive.run();
}

/**
 * Runs the operator control code. This function will be started in its own task
 * with the default priority and stack size whenever the robot is enabled via
 * the Field Management System or the VEX Competition Switch in the operator
 * control mode.
 *
 * If no competition control is connected, this function will run immediately
 * following initialize().
 *
 * If the robot is disabled or communications is lost, the
 * operator control task will be stopped. Re-enabling the robot will restart the
 * task, not resume it from where it left off.
 */

void opcontrol()
{
	pros::Controller master(CONTROLLER_MASTER);

	pros::Motor left_front(LeftFrontPort);
	pros::Motor left_middle(LeftMiddlePort, true);
	pros::Motor left_back(LeftBackPort);

	pros::Motor right_front(RightFrontPort, true);
	pros::Motor right_middle(RightMiddlePort);
	pros::Motor right_back(RightBackPort);

	pros::Motor lift_Front(frontLift, MOTOR_GEARSET_36, true); // Pick correct gearset (36 is red)
	pros::Motor lift_Back(backLift, MOTOR_GEARSET_36, true);

	lift_Front.set_brake_mode(pros::E_MOTOR_BRAKE_HOLD);
	lift_Back.set_brake_mode(pros::E_MOTOR_BRAKE_HOLD);

	pros::c::adi_pin_mode(PneumaticsPort, OUTPUT);
	pros::c::adi_digital_write(PneumaticsPort, HIGH); // write LOW to port 1 (solenoid may be extended or not, depending on wiring)

	bool ConveyorOn = false;
	int dead_Zone = 10; //the deadzone for the joysticks

	while (true)
	{
		/*
		pros::lcd::print(0, "%d %d %d", (pros::lcd::read_buttons() & LCD_BTN_LEFT) >> 2,
						 (pros::lcd::read_buttons() & LCD_BTN_CENTER) >> 1,
						 (pros::lcd::read_buttons() & LCD_BTN_RIGHT) >> 0);
		*/
		int leftSpeed = 0;
		int rightSpeed = 0;
		int analogY = master.get_analog(ANALOG_LEFT_Y);
		int analogX = master.get_analog(ANALOG_RIGHT_X);

		if (analogY == 0 && abs(analogX) > dead_Zone)
		{
			leftSpeed = analogX;
			rightSpeed = -analogX;
		}
		else if (analogX >= dead_Zone && analogY > dead_Zone)
		{
			leftSpeed = analogY;
			rightSpeed = analogY - analogX;
		}
		else if (analogX < -dead_Zone && analogY > dead_Zone)
		{
			leftSpeed = analogY + analogX;
			rightSpeed = analogY;
		}
		else if (analogX >= dead_Zone && analogY < -dead_Zone)
		{
			leftSpeed = analogY;
			rightSpeed = analogY + analogX;
		}
		else if (analogX < -dead_Zone && analogY < -dead_Zone)
		{
			leftSpeed = analogY - analogX;
			rightSpeed = analogY;
		}
		else if (analogX == 0 && abs(analogY) > dead_Zone)
		{
			leftSpeed = analogY;
			rightSpeed = analogY;
		}

		if (master.get_digital(DIGITAL_R1))
		{
			lift_Front.move_velocity(90); //pick a velocity for the lifting
		}
		else if (master.get_digital(DIGITAL_R2))
		{
			lift_Front.move_velocity(-90);
		}
		else
		{
			lift_Front.move_velocity(0);
		}

		if (master.get_digital(DIGITAL_L1) && ConveyorOn == false)
		{
			lift_Back.move_velocity(90); //pick a velocity for the lifting
		}
		else if (master.get_digital(DIGITAL_L2) && ConveyorOn == false)
		{
			lift_Back.move_velocity(-90);
		}
		else if (ConveyorOn == false)
		{
			lift_Back.move_velocity(0);
		}

		if (master.get_digital(DIGITAL_B))
		{
			lift_Back.move_velocity(0);
			ConveyorOn = true;
			pros::c::adi_digital_write(PneumaticsPort, LOW);
			pros::delay(250);
			lift_Back.move_velocity(-100);
		}

		if (master.get_digital(DIGITAL_X))
		{
			lift_Back.move_velocity(0);
			ConveyorOn = true;
			pros::c::adi_digital_write(PneumaticsPort, LOW);
			pros::delay(250);
			lift_Back.move_velocity(100);
		}

		if (master.get_digital(DIGITAL_Y))
		{
			lift_Back.move_velocity(0);
			pros::c::adi_digital_write(PneumaticsPort, HIGH);
			pros::delay(250);
			ConveyorOn = false;
		}
		left_front.move(leftSpeed * 1.574);
		left_middle.move(leftSpeed * 1.574);
		left_back.move(leftSpeed * 1.574);

		right_front.move(rightSpeed * 1.574);
		right_middle.move(rightSpeed * 1.574);
		right_back.move(rightSpeed * -1.574);

		pros::delay(10);
	}
}
