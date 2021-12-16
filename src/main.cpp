#include "main.h"

/**
 * A callback function for LLEMU's center button.
 *
 * When this callback is fired, it will toggle line 2 of the LCD text between
 * "I was pressed!" and nothing.
 */

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

/**
 * Runs initialization code. This occurs as soon as the program is started.
 *
 * All other competition modes are blocked by initialize; it is recommended
 * to keep execution time for this mode under a few seconds.
 */
void initialize()
{
	pros::lcd::initialize();
	pros::lcd::set_text(1, "Hello PROS User!");

	pros::lcd::register_btn1_cb(on_center_button);
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
	//setting all the ports and motors
	pros::Motor left_back{LeftMotor2};
	pros::Motor right_back{RightMotor2};
	pros::Motor left_front{LeftMotor1};
	pros::Motor right_front{RightMotor1};
	pros::Motor middle_right{RightMiddle};
	pros::Motor middle_left{LeftMiddle, true};
	pros::Controller master{CONTROLLER_MASTER};
	pros::Motor lift_Front{frontLift, MOTOR_GEARSET_36, true}; // Pick correct gearset (36 is red)
	pros::Motor lift_Back{backLift, MOTOR_GEARSET_36};

	void Move(int ticks, int speed)
	{
		left_front.move_relative(ticks, speed);
		right_front.move_relative(ticks, speed);
		left_back.move_relative(ticks, speed);
		right_back.move_relative(ticks, speed);
		middle_left.move_relative(ticks, speed);
		middle_right.move_relative(ticks, speed);
	}

public:
	void run()
	{
		lift_Front.set_brake_mode(pros::E_MOTOR_BRAKE_HOLD);

		//moving forward to goal
		Move(4100, 200);

		lift_Front.move_relative(-2900, 100);
		//lift_Back.move_relative(5000, 90);

		pros::delay(1450);

		lift_Front.move_relative(2000, 100);
		pros::delay(350);

		Move(-3000, 200);
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
	pros::Motor left_back(LeftMotor2);
	pros::Motor right_back(RightMotor2);
	pros::Motor left_front(LeftMotor1);
	pros::Motor right_front(RightMotor1, true);
	pros::Motor middle_right(RightMiddle);
	pros::Motor middle_left(LeftMiddle);
	pros::Motor lift_Front(frontLift, MOTOR_GEARSET_36, true); // Pick correct gearset (36 is red)
	pros::Motor lift_Back(backLift, MOTOR_GEARSET_36);
	lift_Front.set_brake_mode(pros::E_MOTOR_BRAKE_HOLD);
	lift_Back.set_brake_mode(pros::E_MOTOR_BRAKE_HOLD);
	bool conveyor_up = false;
	bool conveyor_down = false;
	int dead_Zone = 10; //the deadzone for the joysticks

	while (true)
	{

		pros::lcd::print(0, "%d %d %d", (pros::lcd::read_buttons() & LCD_BTN_LEFT) >> 2,
						 (pros::lcd::read_buttons() & LCD_BTN_CENTER) >> 1,
						 (pros::lcd::read_buttons() & LCD_BTN_RIGHT) >> 0);
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

	if (master.get_digital(DIGITAL_L1))
	{
		lift_Back.move_velocity(90); //pick a velocity for the lifting
	}
	else if (master.get_digital(DIGITAL_L2))
	{
		lift_Back.move_velocity(-90);
	}
	else
	{
		lift_Back.move_velocity(0);
	}

	left_back = leftSpeed * 1.574;
	right_back = rightSpeed * 1.574;
	left_front.move(leftSpeed * 1.574);
	right_front.move(rightSpeed * 1.574);
	middle_left.move(leftSpeed * 1.574);
	middle_right.move(rightSpeed * 1.574);
	pros::delay(10);
}
}