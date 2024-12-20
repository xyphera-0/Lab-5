////////////////////////////////////////////////////////////////////////
//** ENGR-2350 Lab 4 Template
//** NAME: Evan Lacey
//** RIN: 662057116
////////////////////////////////////////////////////////////////////////
//**
//** README!!!!!
//** README!!!!!
//** README!!!!!
//**
//** This template project has all initializations required to both control the motors
//** via PWM and measure the speed of the motors. The PWM is configured using a 24 kHz
//** period (1000 counts). The motors are initialized to be DISABLED and in FORWARD mode.
//** The encoders measurements are stored within the variables TachR and TachL for the
//** right and left motors, respectively. A maximum value for TachR and TachL is
//** enforced to be 1e6 such that when the wheel stops, a reasonable value for the
//** encoders exists: a very large number that can be assumed to be stopped.
//** Finally, a third timer is added to measure a 100 ms period for control system
//** timing. The variable runControl is set to 1 each period and then reset in the main.

#include "engr2350_msp432.h"


void GPIOInit();
void TimerInit();
void Encoder_ISR();
void T2_100ms_ISR();
void I2CInit();
uint16_t readonCarCompass();
uint16_t readHandCompass();


Timer_A_UpModeConfig TA0cfg; // PWM timer
Timer_A_UpModeConfig TA2cfg; // 100 ms timer
Timer_A_ContinuousModeConfig TA3cfg; // Encoder timer
Timer_A_CompareModeConfig TA0_ccr3; // PWM Right
Timer_A_CompareModeConfig TA0_ccr4; // PWM Left
Timer_A_CaptureModeConfig TA3_ccr0; // Encoder Right
Timer_A_CaptureModeConfig TA3_ccr1; // Encoder Left

eUSCI_I2C_MasterConfig I2c;
eUSCI_I2C_MasterConfig I2c2;

float ki = .0001; // integral control gain
float kp = -0.5;
float kd =1;
float speed_val; //   speed  // ADC value from potetiometer (left)
float dir_val, direct_val;   // steering // ADC value from potetiometer (right)
float desired_speed_left, diff_speed_left, min_diff_speed_left, max_diff_speed_left, speed_error_left, speed_error_leftsum,correct_speed_left, compare_value_left, m, left_speed;

float desired_speed_right, diff_speed_right, min_diff_speed_right, max_diff_speed_right, speed_error_right, speed_error_rightsum, correct_speed_right, compare_value_right, m, right_speed;
float max_diff_speed;
float heading_error, desired_heading, measured_heading, previous_error, compass_speed, handcomHeading;
float heading_threshold = 10;
float i,d,c;
uint8_t hand[2];
uint8_t on_car[2];
uint8_t hand_compass[2];
uint8_t on_carcompass[2];
uint8_t compass[2];
uint8_t handcompass[2];
uint8_t handHeading[2];
uint16_t compassHeading;
uint16_t compassSpeed;
int8_t negative;



// Encoder total events
uint32_t enc_total_L,enc_total_R;
// Speed measurement variables
// Note that "Tach" stands for "Tachometer," or a device used to measure rotational speed
int32_t TachL_count, TachL, TachL_sum, TachL_sum_count, TachL_avg; // Left wheel
int32_t TachR_count, TachR, TachR_sum, TachR_sum_count, TachR_avg; // Right wheel
    // TachL,TachR are equivalent to enc_counts from Activity 10/Lab 3
    // TachL/R_avg is the averaged TachL/R value after every 12 encoder measurements
    // The rest are the intermediate variables used to assemble TachL/R_avg

uint8_t runControl = 0; // Flag to denote that 100ms has passed and control should be run.

int main( void ) {    /** Main Function ****/
    SysInit();
    GPIOInit();
    I2CInit();
    TimerInit();


    __delay_cycles(24e6);

    GPIO_setOutputHighOnPin(GPIO_PORT_P3,GPIO_PIN6|GPIO_PIN7);
    GPIO_setOutputLowOnPin(GPIO_PORT_P1,GPIO_PIN0);
    printf("Lab 5 Part a \r\n");

//    readCompass();
//    printf("after read compass");
   // Loop until both compasses report being calibrated:
    while(1){
            //printf("calibrating compass\r\n");
            I2C_readData(EUSCI_B3_BASE, 0x60, 30, on_car, 2);
            I2C_readData(EUSCI_B3_BASE, 0x63, 30, hand, 2);
            printf("on_car[0]:::: %u \r\n", on_car[0]&0x03);
            printf("on_hand[0]:::: %u \r\n", hand[0]&0x03);
            if((on_car[0]&0x03) == (0x07&0x03)){
                printf("on car compass has been calibrate\r\n");
                GPIO_setOutputHighOnPin(GPIO_PORT_P1, GPIO_PIN0);
            } else {
                GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN0);
                printf("Calibrating car\r\n");
            }
            if((hand[0]&0x03) == (0x07&0x03)){
                printf("on hand compass has been calibrate\r\n");
                GPIO_setOutputHighOnPin(GPIO_PORT_P2, GPIO_PIN0);
            }else {
                printf("calibrating hand\r\n");
                GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN0);
            }
            if(((on_car[0]&0x03) == (0x07&0x03)) && ((hand[0]&0x03) == (0x07&0x03))) {
                printf("leaving loop\r\n");

                break;
            }
        }



    printf("Lab 5 Part a \r\n");
    while( 1 ) {
        if(runControl){    // If 100 ms has passed
            runControl = 0;    // Reset the 100 ms flag
            printf("starting to read compass data \r\n");
            I2C_readData(EUSCI_B3_BASE,0x63,4,hand_compass,2);
            I2C_readData(EUSCI_B3_BASE,0x60,2,on_carcompass,2);
            readonCarCompass();
            readHandCompass();
            desired_heading = handcomHeading;
            printf("Compass on car: %4u\r\n",compassHeading);
            printf("Compass on hand heading: %f\r\n",handcomHeading);
            printf("Compass on hand: %4u\r\n",handcompass[0]);
            printf("pitch value: %4u\r\n",handcompass[0]);
            compass_speed=hand_compass[0];
            measured_heading= compassHeading;
            printf("measured: %f \r\n", measured_heading);
//            <-- Start Heading Control -->
            heading_error = desired_heading - measured_heading;
            d = heading_error - previous_error;
            i += heading_error;
            c =  kp * heading_error + ki * i + kd * d;
            if (fabs(heading_error) < heading_threshold) {
                heading_error = 0;
            }
            if(fabs(heading_error) > heading_threshold){
                if (heading_error > 1800) {
                    heading_error-=3600;
                }else if (heading_error < -1800) {
                    heading_error+=3600;
                }
                max_diff_speed = (kp*heading_error)+kd*(heading_error-(heading_error-0.2));
                diff_speed_left=desired_speed_left- max_diff_speed;
                if(max_diff_speed > 720){
                    max_diff_speed = 720;
                }else if(max_diff_speed < -720){
                    max_diff_speed = -720;
                }
                diff_speed_right=desired_speed_right  + max_diff_speed;
                if(diff_speed_left > (.2*max_diff_speed_left)) {
                    compass_speed = max_diff_speed_left;
                }

                previous_error = heading_error;
            }else{

//            <-- End Heading Control -->
//            <-- Start Speed Control -->
//            Calculate the desired speed from the compass tilt value
                left_speed = desired_speed_left - c;
                right_speed = desired_speed_right + c;

//            <-- End Speed Control -->
//            <-- Wheel Speed Control from Lab4 -->
            if(measured_heading ==  desired_heading) {
                if(compass_speed >127){
                    negative= compass_speed-225;
                    printf("negative: %u\r\n", negative);
                }else if(compass_speed < 127 ){
                    negative=compass_speed;
                    printf("negative: %u\r\n", negative);
                }
                if(negative > 0){
                    printf("stright\r\n");
                    GPIO_setOutputLowOnPin(GPIO_PORT_P5, GPIO_PIN4);
                    GPIO_setOutputLowOnPin(GPIO_PORT_P5, GPIO_PIN5);
                    desired_speed_left=(m)*left_speed+200.0;
                    desired_speed_right=(m)*right_speed+200.0;
                    Timer_A_setCompareValue(TIMER_A0_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_4, desired_speed_left);
                    Timer_A_setCompareValue(TIMER_A0_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_3, desired_speed_right);
                }
                if(negative < 0){
                    printf("backwards\r\n");
                    GPIO_setOutputHighOnPin(GPIO_PORT_P5, GPIO_PIN4);
                    GPIO_setOutputHighOnPin(GPIO_PORT_P5, GPIO_PIN5);
                    desired_speed_left = ((-1)*(m))*(8192-(1.1*8192))+200.0;
                    desired_speed_right =(-1*(m))*(8192-(1.1*8192))+200.0;
                    Timer_A_setCompareValue(TIMER_A0_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_4, desired_speed_left);
                    Timer_A_setCompareValue(TIMER_A0_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_3, desired_speed_right);
                }

            }else {
                GPIO_setOutputLowOnPin(GPIO_PORT_P5, GPIO_PIN4);
                Timer_A_setCompareValue(TIMER_A0_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_4, (desired_speed_left+100.0));
                GPIO_setOutputHighOnPin(GPIO_PORT_P5, GPIO_PIN5);
                Timer_A_setCompareValue(TIMER_A0_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_3, desired_speed_right-100.0);
            }
        }
    }
    }
}

void GPIOInit(){
    GPIO_setAsOutputPin(GPIO_PORT_P5,GPIO_PIN4|GPIO_PIN5);   // Motor direction pins
    GPIO_setAsOutputPin(GPIO_PORT_P3,GPIO_PIN6|GPIO_PIN7);   // Motor enable pins
        // Motor PWM pins
    GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P2,GPIO_PIN6|GPIO_PIN7,GPIO_PRIMARY_MODULE_FUNCTION);
        // Motor Encoder pins
    GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P10,GPIO_PIN4|GPIO_PIN5,GPIO_PRIMARY_MODULE_FUNCTION);

    GPIO_setOutputLowOnPin(GPIO_PORT_P5,GPIO_PIN4|GPIO_PIN5);   // Motors set to forward
    GPIO_setOutputHighOnPin(GPIO_PORT_P3,GPIO_PIN6|GPIO_PIN7);   // Motors are OFF
        //potentiometer
//    GPIO_setAsInputPin(GPIO_PORT_P4,GPIO_PIN1|GPIO_PIN4); //4.4 left (speed), 4.1 to right(steering)
    GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P4, GPIO_PIN4, GPIO_TERTIARY_MODULE_FUNCTION); //4.4 left (speed)
    GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P4, GPIO_PIN1, GPIO_TERTIARY_MODULE_FUNCTION); //4.1 to right (steering)

    GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P6, GPIO_PIN6, GPIO_SECONDARY_MODULE_FUNCTION);
    GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P6, GPIO_PIN7, GPIO_SECONDARY_MODULE_FUNCTION);

    GPIO_setAsOutputPin(GPIO_PORT_P2,GPIO_PIN0|GPIO_PIN1|GPIO_PIN2);
    GPIO_setOutputLowOnPin(GPIO_PORT_P2,GPIO_PIN0|GPIO_PIN1|GPIO_PIN2);
    GPIO_setAsOutputPin(GPIO_PORT_P1,GPIO_PIN0);
    GPIO_setOutputLowOnPin(GPIO_PORT_P1,GPIO_PIN0);
}

void I2CInit(){
    I2c.selectClockSource=EUSCI_B_I2C_CLOCKSOURCE_SMCLK;
    I2c.i2cClk = 24000000;
    I2c.dataRate = EUSCI_B_I2C_SET_DATA_RATE_100KBPS;
    I2c.byteCounterThreshold=0;
    I2c.autoSTOPGeneration=EUSCI_B_I2C_NO_AUTO_STOP;
    I2C_initMaster(EUSCI_B3_BASE, &I2c);
    I2C_enableModule(EUSCI_B3_BASE);

}

void TimerInit(){
    // Configure PWM timer for 24 kHz
    TA0cfg.clockSource = TIMER_A_CLOCKSOURCE_SMCLK;
    TA0cfg.clockSourceDivider = TIMER_A_CLOCKSOURCE_DIVIDER_1;
    TA0cfg.timerPeriod = 999;
    Timer_A_configureUpMode(TIMER_A0_BASE,&TA0cfg);

    // Configure TA0.CCR3 for PWM output, Right Motor
    TA0_ccr3.compareRegister = TIMER_A_CAPTURECOMPARE_REGISTER_3;
    TA0_ccr3.compareOutputMode = TIMER_A_OUTPUTMODE_RESET_SET;
    TA0_ccr3.compareValue = 0;
    Timer_A_initCompare(TIMER_A0_BASE,&TA0_ccr3);

    // Configure TA0.CCR4 for PWM output, Left Motor
    TA0_ccr4.compareRegister = TIMER_A_CAPTURECOMPARE_REGISTER_4;
    TA0_ccr4.compareOutputMode = TIMER_A_OUTPUTMODE_RESET_SET;
    TA0_ccr4.compareValue = 0;
    Timer_A_initCompare(TIMER_A0_BASE,&TA0_ccr4);

    // Configure Encoder timer in continuous mode
    TA3cfg.clockSource = TIMER_A_CLOCKSOURCE_SMCLK;
    TA3cfg.clockSourceDivider = TIMER_A_CLOCKSOURCE_DIVIDER_1;
    TA3cfg.timerInterruptEnable_TAIE = TIMER_A_TAIE_INTERRUPT_ENABLE;
    Timer_A_configureContinuousMode(TIMER_A3_BASE,&TA3cfg);

    // Configure TA3.CCR0 for Encoder measurement, Right Encoder
    TA3_ccr0.captureRegister = TIMER_A_CAPTURECOMPARE_REGISTER_0;
    TA3_ccr0.captureMode = TIMER_A_CAPTUREMODE_RISING_EDGE;
    TA3_ccr0.captureInputSelect = TIMER_A_CAPTURE_INPUTSELECT_CCIxA;
    TA3_ccr0.synchronizeCaptureSource = TIMER_A_CAPTURE_SYNCHRONOUS;
    TA3_ccr0.captureInterruptEnable = TIMER_A_CAPTURECOMPARE_INTERRUPT_ENABLE;
    Timer_A_initCapture(TIMER_A3_BASE,&TA3_ccr0);

    // Configure TA3.CCR1 for Encoder measurement, Left Encoder
    TA3_ccr1.captureRegister = TIMER_A_CAPTURECOMPARE_REGISTER_1;
    TA3_ccr1.captureMode = TIMER_A_CAPTUREMODE_RISING_EDGE;
    TA3_ccr1.captureInputSelect = TIMER_A_CAPTURE_INPUTSELECT_CCIxA;
    TA3_ccr1.synchronizeCaptureSource = TIMER_A_CAPTURE_SYNCHRONOUS;
    TA3_ccr1.captureInterruptEnable = TIMER_A_CAPTURECOMPARE_INTERRUPT_ENABLE;
    Timer_A_initCapture(TIMER_A3_BASE,&TA3_ccr1);

    // Register the Encoder interrupt
    Timer_A_registerInterrupt(TIMER_A3_BASE,TIMER_A_CCR0_INTERRUPT,Encoder_ISR);
    Timer_A_registerInterrupt(TIMER_A3_BASE,TIMER_A_CCRX_AND_OVERFLOW_INTERRUPT,Encoder_ISR);

    // Configure 10 Hz timer
    TA2cfg.clockSource = TIMER_A_CLOCKSOURCE_SMCLK;
    TA2cfg.clockSourceDivider = TIMER_A_CLOCKSOURCE_DIVIDER_64;
    TA2cfg.timerInterruptEnable_TAIE = TIMER_A_TAIE_INTERRUPT_ENABLE;
    TA2cfg.timerPeriod = 18749;
    Timer_A_configureUpMode(TIMER_A2_BASE,&TA2cfg);
    Timer_A_registerInterrupt(TIMER_A2_BASE,TIMER_A_CCRX_AND_OVERFLOW_INTERRUPT,T2_100ms_ISR);

    // Start all the timers
    Timer_A_startCounter(TIMER_A0_BASE,TIMER_A_UP_MODE);
    Timer_A_startCounter(TIMER_A2_BASE,TIMER_A_UP_MODE);
    Timer_A_startCounter(TIMER_A3_BASE,TIMER_A_CONTINUOUS_MODE);
}


void Encoder_ISR(){
    // If encoder timer has overflowed...
    if(Timer_A_getEnabledInterruptStatus(TIMER_A3_BASE) == TIMER_A_INTERRUPT_PENDING){
        Timer_A_clearInterruptFlag(TIMER_A3_BASE);
        TachR_count += 65536;
        if(TachR_count >= 1e6){ // Enforce a maximum count to TachR so stopped can be detected
            TachR_count = 1e6;
            TachR = 1e6;
        }
        TachL_count += 65536;
        if(TachL_count >= 1e6){ // Enforce a maximum count to TachL so stopped can be detected
            TachL_count = 1e6;
            TachL = 1e6;
        }
    // Otherwise if the Left Encoder triggered...
    }else if(Timer_A_getCaptureCompareEnabledInterruptStatus(TIMER_A3_BASE,TIMER_A_CAPTURECOMPARE_REGISTER_0)&TIMER_A_CAPTURECOMPARE_INTERRUPT_FLAG){
        Timer_A_clearCaptureCompareInterrupt(TIMER_A3_BASE,TIMER_A_CAPTURECOMPARE_REGISTER_0);
        enc_total_R++;   // Increment the total number of encoder events for the left encoder
        // Calculate and track the encoder count values
        TachR = TachR_count + Timer_A_getCaptureCompareCount(TIMER_A3_BASE,TIMER_A_CAPTURECOMPARE_REGISTER_0);
        TachR_count = -Timer_A_getCaptureCompareCount(TIMER_A3_BASE,TIMER_A_CAPTURECOMPARE_REGISTER_0);
        // Sum values for averaging
        TachR_sum_count++;
        TachR_sum += TachR;
        // If 6 values have been received, average them.
        if(TachR_sum_count == 12){
            TachR_avg = TachR_sum/12;
            TachR_sum_count = 0;
            TachR_sum = 0;
        }
    // Otherwise if the Right Encoder triggered...
    }else if(Timer_A_getCaptureCompareEnabledInterruptStatus(TIMER_A3_BASE,TIMER_A_CAPTURECOMPARE_REGISTER_1)&TIMER_A_CAPTURECOMPARE_INTERRUPT_FLAG){
        Timer_A_clearCaptureCompareInterrupt(TIMER_A3_BASE,TIMER_A_CAPTURECOMPARE_REGISTER_1);
        enc_total_L++;
        TachL = TachL_count + Timer_A_getCaptureCompareCount(TIMER_A3_BASE,TIMER_A_CAPTURECOMPARE_REGISTER_1);
        TachL_count = -Timer_A_getCaptureCompareCount(TIMER_A3_BASE,TIMER_A_CAPTURECOMPARE_REGISTER_1);
        TachL_sum_count++;
        TachL_sum += TachL;
        if(TachL_sum_count == 12){
            TachL_avg = TachL_sum/12;
            TachL_sum_count = 0;
            TachL_sum = 0;
        }
    }
}

void T2_100ms_ISR(){
    Timer_A_clearInterruptFlag(TIMER_A2_BASE);
    runControl = 1;
}

uint16_t readonCarCompass(){
//    Ensure a minimum 2-element uint8_t array exists
//    use I2C_readData to fetch registers 2 and 3 at the same time
    printf("starting to read car compass data \r\n");
    I2C_readData(EUSCI_B3_BASE,0x60,2,compass,2);
//    Combine the received bytes together to form the heading value
      compassHeading = (compass[0]<<8) + (compass[1]) ;
      //printf("compassheading: %u \r\n", compassHeading);
      return compassHeading;
}
uint16_t readHandCompass(){
    printf("starting to read hand compass data \r\n");
//    I2C_readData(EUSCI_B3_BASE,0x63,4,handcompass,2);
    I2C_readData(EUSCI_B3_BASE,0x63,2,handHeading,2);
    handcomHeading = (handHeading[0]<<8) + (handHeading[1]) ;
    return handcomHeading;
}
