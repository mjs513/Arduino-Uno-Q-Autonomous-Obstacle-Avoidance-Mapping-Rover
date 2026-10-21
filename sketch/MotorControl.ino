

// Put this inside your setup() function
void setupMotors() {
  pinMode(DIR_A, OUTPUT);
  pinMode(PWM_A, OUTPUT);
  pinMode(BRAKE_A, OUTPUT);
  
  pinMode(DIR_B, OUTPUT);
  pinMode(PWM_B, OUTPUT);
  pinMode(BRAKE_B, OUTPUT);
}

// --- Main Controller (Unchanged logic, calls updated functions) ---
void motorController() {
  #if defined(DEBUG_MOTOR)
    printf("Mode: %d, throttleLeft: %d, throttleRight: %d\n", currentMode, throttleLeft, throttleRight);
  #endif
  
  // --- MODE 1: FORCED FORWARD ---
  if (currentMode == LOCK_FORWARD) {
    int forwardSpeed = max(throttleLeft, throttleRight);
    
    if (forwardSpeed <= 0) {
      mStop(); 
    } else {
      mForward(forwardSpeed, forwardSpeed); 
    }
  } 
  
  // --- MODE 2: FORCED BACKWARD ---
  else if (currentMode == LOCK_BACKWARD) {
    int backwardSpeed = min(throttleLeft, throttleRight); 
    
    if (backwardSpeed >= 0) {
      mStop();
    } else {
      int absSpeed = abs(backwardSpeed);
      mBackward(absSpeed, absSpeed); 
    }
  } 
  
  // --- MODE 3: STANDARD TANK DRIVE ---
  else {
    int absLeft = abs(throttleLeft);
    int absRight = abs(throttleRight);

    if (throttleLeft == 0 && throttleRight == 0) {
      mStop();
    } 
    else if (throttleLeft > 0 && throttleRight > 0) {
      mForward(absLeft, absRight);
    } 
    else if (throttleLeft < 0 && throttleRight < 0) {
      mBackward(absLeft, absRight);
    } 
    else if (throttleLeft > throttleRight) {
      mRight(absLeft, absRight);
    } 
    else if (throttleRight > throttleLeft) {
      mLeft(absLeft, absRight);
    }
  }
}

//****************** moveForward *******************
void mForward(int speedLeft, int speedRight)
{ 
  int pwmLeft = map(speedLeft, 0, 100, 0, 255 * 0.94);
  int pwmRight = map(speedRight, 0, 100, 0,  255);
    
  mDirection = mFORWARD;

  // Left Motor Forward
  digitalWrite(DIR_A, HIGH);
  digitalWrite(BRAKE_A, LOW);
  analogWrite(PWM_A, pwmLeft);

  // Right Motor Forward
  digitalWrite(DIR_B, HIGH);
  digitalWrite(BRAKE_B, LOW);
  analogWrite(PWM_B, pwmRight);

  #if defined(DEBUG_MOTOR)
    Serial.print("Forward - L: "); Serial.print(pwmLeft); 
    Serial.print(" R: "); Serial.println(pwmRight);
  #endif
}

//****************** moveBackward *******************
void mBackward(int speedLeft, int speedRight)
{
  int pwmLeft = map(speedLeft, 0, 100, 0, 255);
  int pwmRight = map(speedRight, 0, 100, 0, 255);
    
  mDirection = mREVERSE;

  // Left Motor Backward
  digitalWrite(DIR_A, LOW);
  digitalWrite(BRAKE_A, LOW);
  analogWrite(PWM_A, pwmLeft);

  // Right Motor Backward
  digitalWrite(DIR_B, LOW);
  digitalWrite(BRAKE_B, LOW);
  analogWrite(PWM_B, pwmRight);

  #if defined(DEBUG_MOTOR)
    Serial.print("Backward - L: "); Serial.print(pwmLeft); 
    Serial.print(" R: "); Serial.println(pwmRight);
  #endif
}

//****************** turnRight (Pivot) *******************
void mRight(int speedLeft, int speedRight)
{
  int pwmLeft = map(speedLeft, 0, 100, 0, 255);
  int pwmRight = map(speedRight, 0, 100, 0, 255);
    
  mDirection = RIGHT;

  // Left Motor Forward, Right Motor Backward to pivot right
  digitalWrite(DIR_A, HIGH);
  digitalWrite(BRAKE_A, LOW);
  analogWrite(PWM_A, pwmLeft);

  digitalWrite(DIR_B, LOW);
  digitalWrite(BRAKE_B, LOW);
  analogWrite(PWM_B, pwmRight);

  #if defined(DEBUG_MOTOR)
    Serial.print("Pivoting Right - L: "); Serial.print(pwmLeft); 
    Serial.print(" R: "); Serial.println(pwmRight);
  #endif
}

//****************** turnLeft (Pivot) *******************
void mLeft(int speedLeft, int speedRight)
{
  int pwmLeft = map(speedLeft, 0, 100, 0, 255);
  int pwmRight = map(speedRight, 0, 100, 0, 255);
    
  mDirection = LEFT;

  // Left Motor Backward, Right Motor Forward to pivot left
  digitalWrite(DIR_A, LOW);
  digitalWrite(BRAKE_A, LOW);
  analogWrite(PWM_A, pwmLeft);

  digitalWrite(DIR_B, HIGH);
  digitalWrite(BRAKE_B, LOW);
  analogWrite(PWM_B, pwmRight);

  #if defined(DEBUG_MOTOR)
    Serial.print("Pivoting Left - L: "); Serial.print(pwmLeft); 
    Serial.print(" R: "); Serial.println(pwmRight);
  #endif
}

//****************** Stop / Brake *******************
void mStop()
{
  // Engage brakes and clear PWM
  digitalWrite(BRAKE_A, HIGH);
  analogWrite(PWM_A, 0);

  digitalWrite(BRAKE_B, HIGH);
  analogWrite(PWM_B, 0);
    
  mDirection = STOP;

  #if defined(DEBUG_MOTOR)
    Serial.println("Brake");
  #endif
  
  struct odom_state local_odom;

  if (k_mutex_lock(&odom_mutex, K_MSEC(10)) == 0) {
      local_odom = current_odom;
      k_mutex_unlock(&odom_mutex);

      #if defined(DEBUG_ODO)
        printf("Stopped ODO -> X: %f, Y: %f, Heading: %f deg, Pitch: %f deg, Roll: %f deg, EncLeft: %d, EncRight: %d\r\n", 
          local_odom.x, local_odom.y, local_odom.heading, local_odom.pitch, local_odom.roll, 
          local_odom.left_enc, local_odom.right_enc);
      #endif
  }
}

/**********************************************************/
void mForwardPWM(int pwmLeft, int pwmRight)
{ 
  digitalWrite(DIR_A, HIGH);
  digitalWrite(BRAKE_A, LOW);
  analogWrite(PWM_A, pwmLeft);

  digitalWrite(DIR_B, HIGH);
  digitalWrite(BRAKE_B, LOW);
  analogWrite(PWM_B, pwmRight);

  mDirection = mFORWARD;
  
  #if defined(DEBUG_MOTOR)
    Serial.print("Forward - L: "); Serial.print(pwmLeft); 
    Serial.print(" R: "); Serial.println(pwmRight);
  #endif
}