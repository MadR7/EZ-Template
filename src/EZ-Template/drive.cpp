/*
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "main.h"
#include <list>

// !Util



// Set drive
void
Drive::set_left_chassis(int l) {
  for (int i : LL_MOTOR_PORTS) {
    pros::c::motor_move_voltage(abs(i), sgn(i) * l * (12000.0/127.0));
  }
}

void
Drive::set_right_chassis(int r) {
  for (int i : RR_MOTOR_PORTS) {
    pros::c::motor_move_voltage(abs(i), sgn(i) * r * (12000.0/127.0));
  }
}

void
Drive::set_tank(int l, int r) {
  set_left_chassis(l);
  set_right_chassis(r);
}

// Brake modes
void
Drive::set_drive_brake(pros::motor_brake_mode_e_t input) {
  for (int i : LL_MOTOR_PORTS) {
    pros::c::motor_set_brake_mode(abs(i), input);
  }
  for (int i : RR_MOTOR_PORTS) {
    pros::c::motor_set_brake_mode(abs(i), input);
  }
}


// Motor telemetry
void
Drive::reset_drive_sensor() {
  pros::c::motor_tare_position(abs(LL_MOTOR_PORTS.front()));
  pros::c::motor_tare_position(abs(RR_MOTOR_PORTS.front()));
}

int Drive::right_sensor()   { return pros::c::motor_get_position(abs(RR_MOTOR_PORTS.front())); }
int Drive::right_velocity() { return pros::c::motor_get_actual_velocity(abs(RR_MOTOR_PORTS.front())); }

int Drive::left_sensor()    { return pros::c::motor_get_position(abs(LL_MOTOR_PORTS.front())); }
int Drive::left_velocity()  { return pros::c::motor_get_actual_velocity(abs(LL_MOTOR_PORTS.front())); }


void  Drive::tare_gyro() { gyro.set_rotation(0); }
float Drive::get_gyro()  { return gyro.get_rotation(); }


bool
Drive::imu_calibrate() {
  gyro.reset();
  int time = pros::millis();
  int iter = 0;
  int delay = 10;
  while (gyro.get_status() & pros::c::E_IMU_STATUS_CALIBRATING) {
    iter += delay;

    if (iter > 2990) {
      printf("No IMU plugged in, (took %d ms to realize that)\n", iter);
      return false;
    }
    pros::delay(delay);
  }
  master.rumble(".");
  printf("IMU is done calibrating (took %d ms)\n", iter);
  return true;
}

//!Joystick

//bool is_tank;

///
// Input Curve
///



// ! Auton

// Slew
// Variables that are arrays mean the first variable is for forward and the second is for backward
int SLEW_MIN_POWER[2] = {FW_SLEW_MIN_POWER, BW_SLEW_MIN_POWER}; // Starting speed for the slew
int SLEW_DISTANCE [2] = {FW_SLEW_DISTANCE, BW_SLEW_DISTANCE};  // Distance the robot slews at before reaching max speed

const bool DEBUG = false;

const int FORWARD  = 0;
const int BACKWARD = 1;

// Forward Drive Constants
float fw_drive_kp = FW_DRIVE_KP;
float fw_drive_kd = FW_DRIVE_KD;

// Backward Drive Constants
float bw_drive_kp = BW_DRIVE_KP;
float bw_drive_kd = BW_DRIVE_KD;

// Minimum speed for driving and error to stop within
// if spede goes below min_speed, robot travels at min_speed until it gets within min_error, where motors go 0
int min_speed = MIN_SPEED;
int min_error = MIN_ERROR;

// Heading Constants (uses imu to keep robot driving straight)
float heading_kp = HEADING_KP;
float heading_kd = HEADING_KD;

// Turn Constants
float gyro_kp = GYRO_KP;
float gyro_ki = GYRO_KI;
float gyro_kd = GYRO_KD;
int   start_i = START_I; // Start I when error is this
int clipped_turn_i_speed = CLIPPED_TURN_I_SPEED; // When I engages, this becomes max power

// Swing Constants
float swing_kp = SWING_KP;
float swing_kd = SWING_KD;

float drive_constant[2][2] = {
  {fw_drive_kp, fw_drive_kd}, // Foward KP, KD
  {bw_drive_kp, bw_drive_kd}  // Backward KP, KD
};
int direction;

///
// Adjust Constants
///
void
Drive::set_slew_min_power(int fw, int bw) {
  SLEW_MIN_POWER[FORWARD]  = fw;
  SLEW_MIN_POWER[BACKWARD] = bw;
}

void
Drive::set_slew_distance(int fw, int bw) {
  SLEW_DISTANCE[FORWARD]  = fw;
  SLEW_DISTANCE[BACKWARD] = bw;
}

void
Drive::set_fw_drive_constants(float kp, float kd) {
  drive_constant[FORWARD][0] = kp;
  drive_constant[FORWARD][1] = kd;
}

void
Drive::set_bw_drive_constants(float kp, float kd) {
  drive_constant[BACKWARD][0] = kp;
  drive_constant[BACKWARD][1] = kd;
}

void
Drive::set_heading_constants(float kp, float kd) {
  heading_kp = kp;
  heading_kd = kd;
}

void
Drive::set_turn_constants(float kp, float ki, float kd) {
  gyro_kp = kp;
  gyro_ki = ki;
  gyro_kd = kd;
}

void
Drive::set_turn_i_constants(float starting, int clipping) {
  start_i = starting;
  clipped_turn_i_speed = clipping;
}

void
Drive::set_swing_constants(float kp, float kd) {
  swing_kp = kp;
  swing_kd = kd;
}

///
// Reset to default constants
///
void
Drive::reset_slew_min_power() {
  SLEW_MIN_POWER[FORWARD]  = FW_SLEW_MIN_POWER;
  SLEW_MIN_POWER[BACKWARD] = FW_SLEW_MIN_POWER;
}

void
Drive::reset_slew_distance() {
  SLEW_DISTANCE[FORWARD]  = FW_SLEW_DISTANCE;
  SLEW_DISTANCE[BACKWARD] = FW_SLEW_DISTANCE;
}

void
Drive::reset_fw_drive_constants() {
  fw_drive_kp = FW_DRIVE_KP;
  fw_drive_kd = FW_DRIVE_KD;
}

void
Drive::reset_bw_drive_constants() {
  bw_drive_kp = BW_DRIVE_KP;
  bw_drive_kd = BW_DRIVE_KD;
}

void
Drive::reset_heading_constants() {
  heading_kp = HEADING_KP;
  heading_kd = HEADING_KD;
}

void
Drive::reset_turn_constants() {
  gyro_kp = GYRO_KP;
  gyro_ki = GYRO_KI;
  gyro_kd = GYRO_KD;
}

void
Drive::reset_turn_i_constants() {
  start_i = START_I;
  clipped_turn_i_speed = CLIPPED_TURN_I_SPEED;
}

void
Drive::reset_swing_constants() {
  swing_kp = SWING_KP;
  swing_kd = SWING_KD;
}

const float TICK_PER_REV  = (50*(3600/CART_RPM)) * RATIO; // with no cart, the encoder reads 50 counts per rotation
const float CIRCUMFERENCE = WHEEL_DIA*M_PI;
const float TICK_PER_INCH = (TICK_PER_REV/CIRCUMFERENCE);

// "Enumerator" for drive type
int active_drive_type = drive;

bool heading_on = false;
float l_target_encoder, r_target_encoder;
float l_start, r_start;
int max_speed = 0;
float gyro_target = 0;

// Slew variables
int l_x_intercept, r_x_intercept;
int l_y_intercept, r_y_intercept;
int l_sign, r_sign, gyro_sign;
float l_slew_error, r_slew_error;
float l_slope, r_slope;
bool slew = false;

// Swing variables
int swing_sign = 0;
bool stop = false;

// Drive PID with active straight code
// - it makes sure the angle of the robot is what it should be all the way through the movements,
// - turning if needed to keep it going straight
void
Drive::drive_pid_task(void*) {
  float left_error, right_error, gyro_error;
  float last_time, last_l_error, last_r_error, last_gyro_error;
  float l_der, r_der, gyro_der;
  float gyro_integral;
  float right_output, left_output, gyro_output;
  int l_output, r_output;
  bool slow_turn = false;
  while (true) {

    // Math for P
    left_error    = l_target_encoder - left_sensor();
    right_error   = r_target_encoder - right_sensor();
    gyro_error    = gyro_target      - get_gyro();

    // Math for D
    l_der    = left_error  - last_l_error;
    r_der    = right_error - last_r_error;
    gyro_der = gyro_error  - last_gyro_error;

    // Math for I
    if (fabs(gyro_error)<start_i/* && sgn(gyro_error)==gyro_sign*/) {
      slow_turn = true;
      gyro_integral = gyro_integral + gyro_error;
    } else {
      slow_turn = false;
      gyro_integral = 0;
    }

    // Combing P I D
    left_output    = (left_error *drive_constant[direction][0])  + (l_der*drive_constant[direction][1]);
    right_output   = (right_error*drive_constant[direction][0])  + (r_der*drive_constant[direction][1]);
    // Different kP, kI and kD are used for turning, heading and swings
    if (active_drive_type==drive)
      gyro_output = (gyro_error*heading_kp) + (gyro_der*heading_kd);
    else if (active_drive_type==turn)
      gyro_output = (gyro_error*gyro_kp) + (gyro_integral*gyro_ki) + (gyro_der*gyro_kd);
    else if (active_drive_type==l_swing || active_drive_type==r_swing)
      gyro_output = (gyro_error*swing_kp) + (gyro_der*swing_kd);

    // If enabled, slew the drive at the begining so the robot doesn't wheelie
    if (slew) {
      // Error for distance it needs to trigger this code
      l_slew_error = l_x_intercept - left_sensor();
      r_slew_error = r_x_intercept - right_sensor();
      if (active_drive_type==drive) {
        // y=mx+b using everything calculated, where x is error
        if (sgn(l_slew_error) == l_sign) {
          left_output  = (l_slope * l_slew_error) + l_y_intercept;
        }
        if (sgn(r_slew_error) == r_sign) {
          right_output = (r_slope * r_slew_error) + r_y_intercept;
        }
        if (sgn(r_slew_error)!=r_sign && sgn(l_slew_error)!=l_sign) {
          slew = false;
        }
      }
    }

    // Clip the speeds to be slower
    left_output  = clip_num(left_output,  max_speed, -max_speed);
    right_output = clip_num(right_output, max_speed, -max_speed);
    if (active_drive_type==turn) {
      if (!slow_turn)
        gyro_output = clip_num(gyro_output, max_speed, -max_speed);
      else
        gyro_output = clip_num(gyro_output, clipped_turn_i_speed, -clipped_turn_i_speed);
    }
    else if (active_drive_type==l_swing || active_drive_type==r_swing) {
      gyro_output = clip_num(gyro_output, max_speed, -max_speed);
    }

    // Set drive based on drive type
    if (active_drive_type == drive) {
      if (heading_on) {
        l_output = left_output;
        r_output = right_output;
      } else {
        l_output = left_output  + gyro_output;
        r_output = right_output - gyro_output;
      }

      // Setting drive to min_speed
      if (!slew) {
        if (abs(l_output)<min_speed) {
          if (fabs(left_error)>min_error)
            l_output = min_speed * sgn(left_error);
          else
            l_output = 0;
        }
        if (abs(r_output)<min_speed) {
          if (fabs(right_error)>min_error)
            r_output = min_speed * sgn(right_error);
          else
            r_output = 0;
        }
      }
    }

    // Turn
    else if (active_drive_type == turn) {
      l_output =  gyro_output;
      r_output = -gyro_output;
    }

    // L Swing
    else if (active_drive_type == l_swing) {
      l_output = gyro_output;
      r_output = 0;
    }

    // R Swing
    else if (active_drive_type == r_swing) {
      r_output = -gyro_output;
      l_output = 0;
    }

    // Don't run motors in the first 1500 the program is on
    // (while IMU is calibrating)
    if (pros::millis()<1500) {
      set_tank(0, 0);
    } else {
      set_tank(l_output, r_output);
    }

    if (DEBUG && pros::millis()>1500) {
      if (active_drive_type == drive)  printf("le: %f   re: %f   l_der %f\n", left_error, right_error, l_der*drive_constant[direction][1]);
      if (active_drive_type==turn || active_drive_type==l_swing || active_drive_type==r_swing)   printf("output: %f   error: %f   p: %f   i: %f   d: %f\n", gyro_output, gyro_error, gyro_error*gyro_kp, gyro_integral*gyro_ki, gyro_der*gyro_kd);
    }

    last_time       = pros::millis();
    last_l_error    = left_error;
    last_r_error    = right_error;
    last_gyro_error = gyro_error;

    pros::delay(10);
  }
}

void
Drive::set_max_speed(int speed) {
  max_speed = speed;
}

void
Drive::set_drive_pid(int type, float target, int speed, bool slew_on, bool toggle_heading) {
  // Global setup
  active_drive_type = type;
  max_speed = speed;
  slew = slew_on;
  stop = slew;
  heading_on = toggle_heading;

  // If drive or line, set targets to drive
  if (type==drive) {
    printf("Drive Started... Target Value: %f\n", target);
    l_start = left_sensor();
    r_start = right_sensor();

    if (target<l_start && target<r_start) {
      direction = BACKWARD;
    } else {
      direction = FORWARD;
    }

    l_target_encoder = l_start + (target*TICK_PER_INCH);
    r_target_encoder = r_start + (target*TICK_PER_INCH);

    l_sign = sgn(l_target_encoder-left_sensor());
    r_sign = sgn(r_target_encoder-right_sensor());

    l_x_intercept = l_start + (SLEW_DISTANCE[direction]*TICK_PER_INCH);
    r_x_intercept = r_start + (SLEW_DISTANCE[direction]*TICK_PER_INCH);

    l_y_intercept = max_speed * l_sign;
    r_y_intercept = max_speed * r_sign;

    l_slope = (SLEW_MIN_POWER[direction]-max_speed) / ((l_start+(SLEW_DISTANCE[direction]*TICK_PER_INCH))-0);
    r_slope = (SLEW_MIN_POWER[direction]-max_speed) / ((l_start+(SLEW_DISTANCE[direction]*TICK_PER_INCH))-0);
  }

  // If turn, set targets to angle
  else if (type == turn) {
    printf("Turn Started... Target Value: %f\n", target);
    gyro_target = target;
    gyro_sign = sgn(target - get_gyro());
  }

  // If l_turn, set targets to angle
  else if (type == l_swing || type == r_swing) {
    printf("Swing Started... Target Value: %f\n", target);
    gyro_target = target;
    swing_sign = sgn(target-get_gyro());
  }
}

bool
Drive::drive_exit_condition(float l_target, float r_target, int small_timeout, int start_small_counter_within, int big_timeout, int start_big_counter_within, int velocity_timeout, bool wait_until = false) {
  static int i = 0, j = 0, k = 0, g = 0;

  // If the robot gets within the target, make sure it's there for small_timeout amount of time
  if (fabs(l_target-left_sensor())<start_small_counter_within && fabs(r_target-right_sensor())<start_small_counter_within) {
    j+=DELAY_TIME;
    //printf("\nJ: %i", j/10);

    if (j>small_timeout) {
      if (!wait_until) printf("Drive Timed Out");
      else             printf("Drive Wait Until Timed Out");
      printf(" - Small Thresh\n");
      return false;
    }
  }
  else {
    j = 0;
  }

  // If the robot is close to the target, start a timer.  If the robot doesn't get closer within
  // a certain amount of time, exit and continue.
  if (fabs(l_target-left_sensor())<start_big_counter_within && fabs(r_target-right_sensor())<start_big_counter_within) {
    i+=DELAY_TIME;
    //printf("\nI: %i", i/10);

    if (i>big_timeout) {
      if (!wait_until) printf("Drive Timed Out");
      else             printf("Drive Wait Until Timed Out");
      printf(" - Big Thresh\n");
      return false;
    }
  }
  else {
    i = 0;
  }

  if (right_velocity()==0 && left_velocity()==0) {
    k+=DELAY_TIME;
    //printf("\nI: %i", i/10);

    if (k>velocity_timeout) {
      if (!wait_until) printf("Drive Timed Out");
      else             printf("Drive Wait Until Timed Out");
      printf(" - Velocity 0\n");
      return false;
    }
  }
  else {
    k = 0;
  }

  return true;
}

bool
Drive::turn_exit_condition(float target, int small_timeout, int start_small_counter_within, int big_timeout, int start_big_counter_within, int velocity_timeout, bool wait_until = false) {
  static int i, j, k;

  // If the robot gets within the target, make sure it's there for small_timeout amount of time
  if (fabs(target-get_gyro())<start_small_counter_within) {
    j+=DELAY_TIME;
    //printf("\nJ: %i", j/10);

    if (j>small_timeout) {
      if (!wait_until) printf("Turn Timed Out");
      else             printf("Turn Wait Until Timed Out");
      printf(" - Small Thresh\n");
      return false;
    }
  }
  else {
    j = 0;
  }
  // If the robot is close to the target, start a timer.  If the robot doesn't get closer within
  // a certain amount of time, exit and continue.
  if (fabs(target-get_gyro())<start_big_counter_within) {
    i+=DELAY_TIME;
    //printf("\nI: %i", i/10);

    if (i>big_timeout) {
      if (!wait_until) printf("Turn Timed Out");
      else             printf("Turn Wait Until Timed Out");
      printf(" - Big Thresh\n");
      return false;
    }
  }
  else {
    i = 0;
  }

  if (right_velocity()==0 && left_velocity()==0) {
    k+=DELAY_TIME;
    //printf("\nI: %i", i/10);

    if (k>velocity_timeout) {
      if (!wait_until) printf("Turn Timed Out");
      else             printf("Turn Wait Until Timed Out");
      printf(" - Velocity 0\n");
      return false;
    }
  }
  else {
    k = 0;
  }
  return true;
}

//Wait for drive
void
Drive::wait_drive() {
  pros::delay(DELAY_TIME);

  // Parameters for exit condition function:
  // #3 - time the robot has to be within #2 of target
  // #4 - threshold for timer to start
  // #5 - time for if position is never reached
  // #6 - position for robot to be within to never reach target
  // #7 - velocity timeout

  if (active_drive_type==drive) {
    while (drive_exit_condition(l_target_encoder, r_target_encoder, 80, 50, 300, 150, 500)) {
      pros::delay(DELAY_TIME);
    }
  }
  else if (active_drive_type==turn) {
    while (turn_exit_condition(gyro_target, 100, 3, 500, 7, 500)) {
      pros::delay(DELAY_TIME);
    }
  }
  else if (active_drive_type==l_swing || active_drive_type==r_swing) {
    while (turn_exit_condition(gyro_target, 100, 3, 500, 7, 500)) {
      pros::delay(DELAY_TIME);
    }
    //while (turn_exit_condition(100, 3, 500, 7) && fabs(lf.get_power()-rf.get_power())<20) {
    //  pros::delay(delay_time);
    //}
  }
}

// Function to wait until a certain position is reached
void
Drive::wait_until(int input) {
  // If robot is driving...
  if (active_drive_type == drive) {
    // Calculate error between current and target (target needs to be an inbetween position)
    int l_tar   = l_start + (input*TICK_PER_INCH);
    int r_tar   = r_start + (input*TICK_PER_INCH);
    int l_error = l_tar   - left_sensor();
    int r_error = r_tar   - right_sensor();
    int l_sgn   = sgn(l_error);
    int r_sgn   = sgn(r_error);
    bool run    = true;

    while (run) {
      l_error = l_tar - left_sensor();
      r_error = r_tar - right_sensor();

      // Break the loop once target is passed
      if (sgn(l_error)==l_sgn && sgn(r_error)==r_sgn) {
        run = true; // this makes sure that the following else if is rnu after the sgn is flipped
      }
      else if (sgn(l_error)!=l_sgn && sgn(r_error)!=r_sgn) {
        printf("Drive Wait Until Completed- Error Sgn Flipped\n");
        run = false;
      }
      else if (!drive_exit_condition(l_tar, r_tar, 80, 50, 300, 150, 500, true)) {
        run = false;
      }

      pros::delay(DELAY_TIME);
    }
  }

  // If robot is turning...
  else if (active_drive_type==turn || active_drive_type==l_swing || active_drive_type==r_swing) {
    // Calculate error between current and target (target needs to be an inbetween position)
    int g_error = input - get_gyro();
    int g_sgn   = sgn(g_error);
    bool run    = true;

    while (run) {
      g_error = input - get_gyro();

      // Break the loop once target is passed
      if (sgn(g_error)==g_sgn) {
        run = true; // this makes sure that the following else if is rnu after the sgn is flipped
      }
      else if (sgn(g_error)!=g_sgn) {
        run = false;
      }
      else if (!turn_exit_condition(input, 100, 3, 500, 7, 500, true)) {
        run = false;
      }

      pros::delay(DELAY_TIME);
    }
  }
}
Drive(std::vector<int> pLL_MOTOR_PORTS, std::vector<int> pRR_MOTOR_PORTS, int pIMU_PORT, pros::controller_id_e_t pController) {
  // initializes motor reversing
  LL_MOTOR_PORTS.assign(pLL_MOTOR_PORTS.begin(), pLL_MOTOR_PORTS.end());
  RR_MOTOR_PORTS.assign(pRR_MOTOR_PORTS.begin(), pRR_MOTOR_PORTS.end());
  // declares imu
  pros::IMU gyro(pIMU_PORT);
  // gets drive_pid_task going
  drive_pid(drive_pid_task, nullptr, "drive_pid");
}
