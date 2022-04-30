#include "main.h"
#include "pros/adi.h"
#include "pros/api_legacy.h"
#include "pros/llemu.hpp"
#include "pros/vision.hpp"
#include "pros/vision.h"


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

#define max(a,b) ((a) < (b) ? (b) : (a))
#define min(a,b) ((a) < (b) ? (a) : (b))
#define sign(x) ((x) > 0 ? 1 : -1)

int autonSide; // 1 is right goal rush + auton point, 2 is left goal rush, 3 is right wings goal rush

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
		pros::lcdset_text(2, "I was pressed!");
	}
	else
	{
		pros::lcdclear_line(2);
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

	pros::lcd::initialize();
	if (pros::lcd::read_buttons() == 4)
	{
		autonSide = 1;
	}
	else if (pros::lcd::read_buttons() == 2)
	{
		autonSide = 2;
	}
	if (autonSide == 1)
	{
		pros::lcd::set_text(1, "Selected Auton is Right");
	}
	if (autonSide == 2)
	{
		pros::lcd::set_text(1, "Selected Auton is Left");
	}

	pros::c::adi_pin_mode(SideArmLeftPort, OUTPUT);
	pros::c::adi_digital_write(SideArmLeftPort, LOW);
	pros::c::adi_pin_mode(SideArmRightPort, OUTPUT);
	pros::c::adi_digital_write(SideArmRightPort, LOW);
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
	pros::Vision vision_sensor{VisionPort, pros::E_VISION_ZERO_CENTER};


	int getLeftPos()
	{
		return (left_front.get_position() + left_middle.get_position() + left_back.get_position()) / 3;
	}

	int getRightPos()
	{
		return (right_front.get_position() + right_middle.get_position() + right_back.get_position()) / 3;
	}

	int getPos()
	{
		return (getLeftPos() + getRightPos()) / 2;
	}
	void Move(int ticks, int Lspeed, int Rspeed, bool FLiftOn, int FTicks, int FSpeed)
	{
		int startPos = getPos();
		int LiftstartPos = lift_Front.get_position();
		left_front.move(Lspeed * 127 / 200);
		left_middle.move(Lspeed * 127 / 200);
		left_back.move(Lspeed * 127 / 200);
		right_front.move(Rspeed * 127 / 200);
		right_middle.move(Rspeed * 127 / 200);
		right_back.move(-Rspeed * 127 / 200);
		lift_Front.move(FSpeed);
		while (abs(getPos() - startPos) < ticks)
		{
			if (abs(lift_Front.get_position() - LiftstartPos) == FTicks)
			{
				lift_Front.move(0);
			}
			pros::c::delay(10);
		}
		left_front.move(0);
		left_middle.move(0);
		left_back.move(0);
		right_front.move(0);
		right_middle.move(0);
		right_back.move(0);

		lift_Front.move(0);
		pros::c::delay(100);
	}

	void Turn(double degrees, int speed)
	{
		left_front.move_relative((degrees / 360) * 3525, speed);
		left_middle.move_relative((degrees / 360) * 3525, speed);
		left_back.move_relative((degrees / 360) * 3525, speed);

		right_front.move_relative((degrees / 360) * -3525, speed);
		right_middle.move_relative((degrees / 360) * -3525, speed);
		right_back.move_relative((degrees / 360) * 3525, speed);
	}
	void MoveVisionAssisted(int ticks, int speed, bool BLiftOn, int BTicks, int BSpeed) {
		int startPos = getPos();
		int BLiftstartPos = lift_Back.get_position();

		while (abs(getPos() - startPos) < ticks) {
			int Lspeed = speed;
			int Rspeed = speed;

			if (vision_sensor.get_object_count() > 0) {
				pros::vision_object_s_t obj;
				
				if(vision_sensor.read_by_size(0, 1, &obj) == 1 && obj.top_coord + obj.height > 0) {
					// Positive offset means goal is left due to sensor being mounted upside down
					float offset = obj.x_middle_coord * sign(speed);
					printf("%d  %d\n", obj.width, obj.top_coord + obj.height);
					if(abs(offset) > 0.05) {
						if (offset < 0){
							Rspeed = speed * (1 + max(offset, -100) * 0.005);
							// Rspeed = -50;
							// Lspeed = 50;
						}
						else {
							Lspeed = speed * (1 - min(offset, 100) * 0.005);
							// Rspeed = 50;
							// Lspeed = -50;
						}
					}
					if (obj.width > 280) {    //checks if the object is too close
						break;
					}            
				}
			}	

			left_front.move(Lspeed * 127 / 200);           
			left_middle.move(Lspeed * 127 / 200);
			left_back.move(Lspeed * 127 / 200);
			right_front.move(Rspeed * 127 / 200);
			right_middle.move(Rspeed * 127 / 200);
			right_back.move(-Rspeed * 127 / 200);

			lift_Back.move(BSpeed);
			if (abs(lift_Back.get_position() - BLiftstartPos) == BTicks)
			{
				lift_Back.move(0);
			}

			pros::c::delay(10);		
		}

		left_front.move(0);
		left_middle.move(0);
		left_back.move(0);
		right_front.move(0);
		right_middle.move(0);
		right_back.move(0);
	}

public:
	void run()
	{

		if (autonSide == 1)
		{
			lift_Front.set_brake_mode(pros::E_MOTOR_BRAKE_HOLD);
			pros::c::adi_pin_mode(ConveyorPort, OUTPUT);
			pros::c::adi_digital_write(ConveyorPort, HIGH);
			pros::c::adi_pin_mode(SideArmLeftPort, OUTPUT);
			pros::c::adi_digital_write(SideArmLeftPort, LOW);
			pros::c::adi_pin_mode(SideArmRightPort, OUTPUT);
			pros::c::adi_digital_write(SideArmRightPort, LOW);

			// moving forward to goal and picking it up
			Move(2000, 200, 200, true, 1700, -200);
			Move(260, 10, 150, true, 0, 0);
			lift_Front.move_relative(10000, 100);
			pros::delay(1300);
			lift_Back.move_relative(-2100, 100);
			pros::delay(1400);
			Move(1200, -150, -150, false, 0, 0);
			pros::delay(50);
			lift_Back.move_relative(1150, 100);
			pros::delay(1300);
			Move(600, 150, 150, false, 0, 0);
			Turn(-145, 150);
			pros::delay(1400);
			// start conveyor, move towards placed rings, pick up rings
			pros::c::adi_digital_write(ConveyorPort, LOW);
			pros::delay(250);
			lift_Back.move_velocity(-100);
			pros::delay(250);
			Move(2250, 100, 100, false, 0, 0);
			pros::delay(500);
			// move back
			Move(1000, -100, -100, false, 0, 0);
			pros::delay(1500);
			Move(1000, 100, 100, false, 0, 0);
			pros::delay(500);
			// move back
			Move(1000, -100, -100, false, 0, 0);
			pros::c::adi_digital_write(ConveyorPort, HIGH);
		}
		else if (autonSide == 2)
		{
			lift_Front.set_brake_mode(pros::E_MOTOR_BRAKE_HOLD);
			pros::c::adi_pin_mode(ConveyorPort, OUTPUT);
			pros::c::adi_digital_write(ConveyorPort, HIGH);
			pros::c::adi_pin_mode(SideArmLeftPort, OUTPUT);
			pros::c::adi_digital_write(SideArmLeftPort, LOW);
			pros::c::adi_pin_mode(SideArmRightPort, OUTPUT);
			pros::c::adi_digital_write(SideArmRightPort, LOW);
			// moving forward to goal and picking it up
			Move(2000, 200, 200, true, 1700, -200);
			Move(500, 160, 10, true, 1000, 100);
			lift_Front.move_relative(10000, 100);
			pros::delay(1300);
			Move(500, -160, -160, true, 0, 0);
			pros::delay(200);
			Turn(-67, 150);
			pros::delay(1000);
			lift_Back.move_relative(-2100, 100);
		}
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

	pros::c::adi_pin_mode(ConveyorPort, OUTPUT);
	pros::c::adi_digital_write(ConveyorPort, HIGH); // write LOW to port 1 (solenoid may be extended or not, depending on wiring)

	pros::c::adi_pin_mode(SideArmLeftPort, OUTPUT);
	pros::c::adi_digital_write(SideArmLeftPort, LOW);
	pros::c::adi_pin_mode(SideArmRightPort, OUTPUT);
	pros::c::adi_digital_write(SideArmRightPort, LOW);
	bool SideArmsDown = false;

	bool ConveyorOn = false;
	int dead_Zone = 10; // the deadzone for the joysticks

	while (true)
	{
		if (pros::lcd::read_buttons() == 4)
		{
			autonSide = 1;
		}
		else if (pros::lcd::read_buttons() == 2)
		{
			autonSide = 2;
		}
		if (autonSide == 1)
		{
			pros::lcd::set_text(1, "Selected Auton is Right");
		}
		if (autonSide == 2)
		{
			pros::lcd::set_text(1, "Selected Auton is Left");
		}

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
			lift_Front.move_velocity(100); // pick a velocity for the lifting
		}
		else if (master.get_digital(DIGITAL_R2))
		{
			lift_Front.move_velocity(-100);
		}
		else
		{
			lift_Front.move_velocity(0);
		}

		if (master.get_digital(DIGITAL_L1) && ConveyorOn == false)
		{
			lift_Back.move_velocity(100); // pick a velocity for the lifting
		}
		else if (master.get_digital(DIGITAL_L2) && ConveyorOn == false)
		{
			lift_Back.move_velocity(-100);
		}
		else if (ConveyorOn == false)
		{
			lift_Back.move_velocity(0);
		}

		if (master.get_digital(DIGITAL_B))
		{
			lift_Back.move_velocity(0);
			ConveyorOn = true;
			pros::c::adi_digital_write(ConveyorPort, LOW);
			pros::delay(250);
			lift_Back.move_velocity(-100);
		}

		if (master.get_digital(DIGITAL_X))
		{
			lift_Back.move_velocity(0);
			ConveyorOn = true;
			pros::c::adi_digital_write(ConveyorPort, LOW);
			pros::delay(250);
			lift_Back.move_velocity(100);
		}

		if (master.get_digital(DIGITAL_Y))
		{
			lift_Back.move_velocity(0);
			pros::c::adi_digital_write(ConveyorPort, HIGH);
			pros::delay(250);
			ConveyorOn = false;
		}
		if (master.get_digital_new_press(DIGITAL_A))
		{
			if (SideArmsDown == false)
			{
				pros::c::adi_digital_write(SideArmLeftPort, HIGH);
				pros::c::adi_digital_write(SideArmRightPort, HIGH);
				SideArmsDown = true;
			}
			else
			{
				pros::c::adi_digital_write(SideArmLeftPort, LOW);
				pros::c::adi_digital_write(SideArmRightPort, LOW);
				SideArmsDown = false;
			}
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
