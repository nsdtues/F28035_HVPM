/* ==============================================================================
System Name:  	PM_Sensorless

File Name:	  	PM_Sensorless.C

Description:	Primary system file for the Real Implementation of Sensorless  
          		Field Orientation Control for Three Phase Permanent-Magnet
          		Synchronous Motor(s) (PMSM) 

Originator:		Digital control systems Group - Texas Instruments

Note: In this software, the default inverter is supposed to be DRV8412-EVM kit. 
=====================================================================================
 History: 04-9-2010	Version 1.1: Support F2803x 
=================================================================================  */

// Include header files used in the main function

#include "PeripheralHeaderIncludes.h"
#include "PM_Sensorless-Settings.h"
#include "IQmathLib.h"
#include "PM_Sensorless.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#ifdef DRV8301
union DRV8301_STATUS_REG_1 DRV8301_stat_reg1;
union DRV8301_STATUS_REG_2 DRV8301_stat_reg2;
union DRV8301_CONTROL_REG_1 DRV8301_cntrl_reg1;
union DRV8301_CONTROL_REG_2 DRV8301_cntrl_reg2;
Uint16 read_drv_status = 0;
#endif

// Prototype statements for functions found within this file.
interrupt void MainISR(void);
void DeviceInit();
void MemCopy();
void InitFlash();
void InitSci(void);
interrupt void sciaRxFifoIsr(void);
interrupt void sciaTxFifoIsr(void);
interrupt void scibRxFifoIsr(void);
interrupt void scibTxFifoIsr(void);
interrupt void timer0_100ms_isr(void);

//user begin
void user_adc_init(void);
void user_qep_init(void);
void user_pwm_init(void);
void user_epwm_init(void);
void user_drv8301_init(void);
//user end

// State Machine function prototypes
//------------------------------------
// Alpha states
void A0(void);	//state A0
void B0(void);	//state B0
void C0(void);	//state C0

// A branch states
void A1(void);	//state A1
void A2(void);	//state A2
void A3(void);	//state A3

// B branch states
void B1(void);	//state B1
void B2(void);	//state B2
void B3(void);	//state B3

// C branch states
void C1(void);	//state C1
void C2(void);	//state C2
void C3(void);	//state C3

// Variable declarations
void (*Alpha_State_Ptr)(void);	// Base States pointer
void (*A_Task_Ptr)(void);		// State pointer A branch
void (*B_Task_Ptr)(void);		// State pointer B branch
void (*C_Task_Ptr)(void);		// State pointer C branch

// Used for running BackGround in flash, and ISR in RAM
extern Uint16 *RamfuncsLoadStart, *RamfuncsLoadEnd, *RamfuncsRunStart;

int16	VTimer0[4];			// Virtual Timers slaved off CPU Timer 0 (A events)
int16	VTimer1[4]; 		// Virtual Timers slaved off CPU Timer 1 (B events)
int16	VTimer2[4]; 		// Virtual Timers slaved off CPU Timer 2 (C events)
int16	SerialCommsTimer;

// Global variables used in this system

_iq VdTesting = _IQ(0.0);			// Vd reference (pu) 
_iq VqTesting = _IQ(0.2);			// Vq reference (pu) 

_iq IdRef = _IQ(0.0);				// Id reference (pu) 
_iq IqRef = _IQ(0.1);				// Iq reference (pu) 
_iq SpeedRef = _IQ(0.25);			// Speed reference (pu)	

_iq cal_offset_A = _IQ15(0.4890);		//F28069	 
_iq cal_offset_B = _IQ15(0.4877);		//F28069
//_iq cal_offset_A = _IQ15(0.4990);		//F28035	 
//_iq cal_offset_B = _IQ15(0.5034);		//F28035
//_iq cal_offset_A = _IQ15(0.5);	 
//_iq cal_offset_B = _IQ15(0.5);
_iq cal_filt_gain;	 

float32 T = 0.001/ISR_FREQUENCY;    // Samping period (sec), see parameter.h 

Uint32 IsrTicker = 0;
Uint16 BackTicker = 0;
Uint16 lsw=0;

int16 PwmDacCh1=0;
int16 PwmDacCh2=0;
int16 PwmDacCh3=0;
int16 PwmDacCh4=0;

int16 DlogCh1 = 0;
int16 DlogCh2 = 0;
int16 DlogCh3 = 0;
int16 DlogCh4 = 0;


#if (BUILDLEVEL==LEVEL1)	 
Uint16 DRV_RESET = 1;
#else
Uint16 DRV_RESET = 0;
#endif

volatile Uint16 EnableFlag = FALSE;
Uint16 LockRotorFlag = FALSE;
Uint16 RunMotor = FALSE;

Uint16 SpeedLoopPrescaler = 10;      // Speed loop prescaler
Uint16 SpeedLoopCount = 1;           // Speed loop counter

//	Instance a position estimator
SMOPOS smo1 = SMOPOS_DEFAULTS;

// Instance a sliding-mode position observer constant Module
SMOPOS_CONST smo1_const = SMOPOS_CONST_DEFAULTS;

// Instance a QEP interface driver 
QEP qep1 = QEP_DEFAULTS; 

// Instance a few transform objects
CLARKE clarke1 = CLARKE_DEFAULTS;
PARK park1 = PARK_DEFAULTS;
IPARK ipark1 = IPARK_DEFAULTS;

// Instance PID regulators to regulate the d and q  axis currents, and speed
PID_GRANDO_CONTROLLER pid1_id = {PID_TERM_DEFAULTS,PID_PARAM_DEFAULTS,PID_DATA_DEFAULTS};
PID_GRANDO_CONTROLLER pid1_iq = {PID_TERM_DEFAULTS,PID_PARAM_DEFAULTS,PID_DATA_DEFAULTS};
PID_GRANDO_CONTROLLER pid1_spd = {PID_TERM_DEFAULTS,PID_PARAM_DEFAULTS,PID_DATA_DEFAULTS};

// Instance a PWM driver instance
PWMGEN pwm1 = PWMGEN_DEFAULTS;

// Instance a PWM DAC driver instance
PWMDAC pwmdac1 = PWMDAC_DEFAULTS;

// Instance a Space Vector PWM modulator. This modulator generates a, b and c
// phases based on the d and q stationery reference frame inputs
SVGENDQ svgen_dq1 = SVGENDQ_DEFAULTS;

// Instance a ramp controller to smoothly ramp the frequency
RMPCNTL rc1 = RMPCNTL_DEFAULTS;

//	Instance a ramp generator to simulate an Anglele
RAMPGEN rg1 = RAMPGEN_DEFAULTS;

//	Instance a phase voltage calculation
PHASEVOLTAGE volt1 = PHASEVOLTAGE_DEFAULTS;

// Instance a speed calculator based on QEP
SPEED_MEAS_QEP speed1 = SPEED_MEAS_QEP_DEFAULTS;

// Instance a speed calculator based on sliding-mode position observer
SPEED_ESTIMATION speed3 = SPEED_ESTIMATION_DEFAULTS;

// Create an instance of DATALOG Module
DLOG_4CH dlog = DLOG_4CH_DEFAULTS;      




void main(void)
{
	
	DeviceInit();	// Device Life support & GPIO		

// Only used if running from FLASH
// Note that the variable FLASH is defined by the compiler
// (see TwoChannelBuck.pjt file)
#ifdef FLASH
// Copy time critical code and Flash setup code to RAM
// The  RamfuncsLoadStart, RamfuncsLoadEnd, and RamfuncsRunStart
// symbols are created by the linker. Refer to the linker files. 
	MemCopy(&RamfuncsLoadStart, &RamfuncsLoadEnd, &RamfuncsRunStart);

// Call Flash Initialization to setup flash waitstates
// This function must reside in RAM
	InitFlash();	// Call the flash wrapper init function
#endif //(FLASH)

   // Waiting for enable flag set


	EALLOW;  // This is needed to write to EALLOW protected registers
	PieVectTable.SCIRXINTA = &sciaRxFifoIsr;
//	PieVectTable.SCITXINTA = &sciaTxFifoIsr;
	PieVectTable.SCIRXINTB = &scibRxFifoIsr;
//  PieVectTable.SCITXINTB = &scibTxFifoIsr;
	PieVectTable.TINT0 = &timer0_100ms_isr;
	EDIS;   // This is needed to disable write to EALLOW protected registers

    InitSci();
    InitCpuTimers();

    user_adc_init();
    user_qep_init();

	PieCtrlRegs.PIEIER9.bit.INTx1=1;     // PIE Group 9, INT1
//	PieCtrlRegs.PIEIER9.bit.INTx2=1;     // PIE Group 9, INT1
	PieCtrlRegs.PIEIER9.bit.INTx3=1;     // PIE Group 9, INT1
//	PieCtrlRegs.PIEIER9.bit.INTx4=1;     // PIE Group 9, INT1
    IER |= M_INT9; // Enable CPU INT

	PieCtrlRegs.PIEIER1.bit.INTx1 = 1;  // Enable INT 1.1 in the PIE
//	PieCtrlRegs.PIEIER9.bit.INTx2=1;     // PIE Group 9, INT2

	IER |= M_INT1; // Enable CPU INT
	EINT;


	ConfigCpuTimer(&CpuTimer0, 80, 100000);
	ConfigCpuTimer(&CpuTimer1, 80, 1000);

	StartCpuTimer0();
	StartCpuTimer1();

	int16 data[64];
	uint16_t adc_result_temp[16];
	memset(data,0,sizeof(data));
	data[0] = 0xaa;

	user_epwm_init();
	user_pwm_init();
    user_drv8301_init();

    while (EnableFlag==FALSE)
	{
	    if(CpuTimer1Regs.TCR.bit.TIF == 1){
	        if(GpioDataRegs.GPADAT.bit.GPIO18 == 0)
	        {
	            DRV8301_stat_reg1.all = DRV8301_SPI_Read(&SpiaRegs,STAT_REG_1_ADDR);
	            DRV8301_stat_reg2.all = DRV8301_SPI_Read(&SpiaRegs,STAT_REG_2_ADDR);
	        }

	        QEP_MACRO(qep1);

	        float electdegree, mechdegree;

	        electdegree = _IQtoF(qep1.ElecTheta);
	        mechdegree = _IQtoF(qep1.MechTheta);

	        int32 qep_cnt = 0;
	        qep_cnt = EQep1Regs.QPOSCNT;

	        clarke1.As=(((AdcResult.ADCRESULT0)*0.00024414-cal_offset_A)*2); // Phase A curr.
	        clarke1.Bs=(((AdcResult.ADCRESULT1)*0.00024414-cal_offset_B)*2); // Phase B curr.

	        park1.Alpha = clarke1.Alpha;
	        park1.Beta = clarke1.Beta;
	        park1.Angle = qep1.ElecTheta;
	        park1.Sine = _IQsinPU(park1.Angle);
	        park1.Cosine = _IQcosPU(park1.Angle);
	        PARK_MACRO(park1)

	        //start pid
	        pid1_id.param.Kp = _IQ(3.176*BASE_CURRENT/BASE_VOLTAGE);
	        pid1_id.term.Out = park1.Ds * pid1_id.param.Kp;
	        if(pid1_id.term.Out > 1){
	            pid1_id.term.Out = 1;
	        }else if(pid1_id.term.Out < -1>){
	            pid1_id.term.Out = -1;
	        }
	        //end pid

//            ipark1.Ds = VdTesting;
            ipark1.Ds = pid1_id.term.Out;
            ipark1.Qs = VqTesting;
            ipark1.Sine = park1.Sine;
            ipark1.Cosine = park1.Cosine;
            IPARK_MACRO(ipark1)

	        svgen_dq1.Ualpha = ipark1.Alpha;
	        svgen_dq1.Ubeta = ipark1.Beta;
	        SVGEN_MACRO(svgen_dq1)

	        pwm1.MfuncC1 = _IQtoQ15(svgen_dq1.Ta);
	        pwm1.MfuncC2 = _IQtoQ15(svgen_dq1.Tb);
	        pwm1.MfuncC3 = _IQtoQ15(svgen_dq1.Tc);
	        PWM_MACRO(pwm1)                                // Calculate the new PWM compare values

	        EPwm1Regs.CMPA.half.CMPA=pwm1.PWM1out;  // PWM 1A - PhaseA
	        EPwm2Regs.CMPA.half.CMPA=pwm1.PWM2out;  // PWM 2A - PhaseB
	        EPwm3Regs.CMPA.half.CMPA=pwm1.PWM3out;  // PWM 3A - PhaseC

	        adc_result_temp[0] = AdcResult.ADCRESULT0;
	        adc_result_temp[1] = AdcResult.ADCRESULT1;
	        int16 i_a1 = (int16)((adc_result_temp[1] - 2048)*0.00083*10);
	        adc_result_temp[2] = AdcResult.ADCRESULT2;
            int16 i_b1 = (int16)((adc_result_temp[2] - 2048)*0.00083*10);
	        adc_result_temp[3] = AdcResult.ADCRESULT3;
            int16 i_c1 = (int16)((adc_result_temp[3] - 2048)*0.00083*10);
	        adc_result_temp[4] = AdcResult.ADCRESULT4;
            Uint16 v_a1 = (Uint16)(adc_result_temp[4]*0.00585*10);
	        adc_result_temp[5] = AdcResult.ADCRESULT5;
	        Uint16 v_b1 = (Uint16)(adc_result_temp[5]*0.00585*10);
	        adc_result_temp[6] = AdcResult.ADCRESULT6;
	        Uint16 v_c1 = (Uint16)(adc_result_temp[6]*0.00585*10);
	        adc_result_temp[7] = AdcResult.ADCRESULT7;
	        Uint16 v_dc = (Uint16)(adc_result_temp[7]*0.00585*10);



	        CpuTimer1Regs.TCR.bit.TIF = 1;   // clear flag
	        BackTicker++;

//	        while(!SciaRegs.SCICTL2.bit.TXRDY);
//	        SciaRegs.SCITXBUF = 0x53;
//	        SciaRegs.SCITXBUF = 0x02;
	        static int tx_cnt = 0;
	        tx_cnt++;
	        if(tx_cnt > 10){
	            while(!ScibRegs.SCICTL2.bit.TXRDY);
	            ScibRegs.SCITXBUF = 0x53;
//	            ScibRegs.SCITXBUF = 0x02;
	            ScibRegs.SCITXBUF = i_a1>>8&0xff;
	            ScibRegs.SCITXBUF = i_a1&0xff;
	            ScibRegs.SCITXBUF = i_b1>>8&0xff;
                ScibRegs.SCITXBUF = i_b1&0xff;
	            ScibRegs.SCITXBUF = i_c1>>8&0xff;
                ScibRegs.SCITXBUF = i_c1&0xff;
	            ScibRegs.SCITXBUF = v_a1>>8&0xff;
                ScibRegs.SCITXBUF = v_a1&0xff;
	            ScibRegs.SCITXBUF = v_b1>>8&0xff;
                ScibRegs.SCITXBUF = v_b1&0xff;
	            ScibRegs.SCITXBUF = v_c1>>8&0xff;
                ScibRegs.SCITXBUF = v_c1&0xff;
                ScibRegs.SCITXBUF = v_dc>>8&0xff;
                ScibRegs.SCITXBUF = v_dc&0xff;
	            tx_cnt = 0;
	        }


//	        GpioDataRegs.GPBTOGGLE.bit.GPIO39 = 1;    // Blink LED
//	        GpioDataRegs.GPBSET.bit.GPIO39 = 1;
	    }
//      DELAY_US(500000);
	}

// Timing sync for slow background tasks 
// Timer period definitions found in device specific PeripheralHeaderIncludes.h
	CpuTimer0Regs.PRD.all =  mSec1;		// A tasks
	CpuTimer1Regs.PRD.all =  mSec5;		// B tasks
	CpuTimer2Regs.PRD.all =  mSec50;	// C tasks

// Tasks State-machine init
	Alpha_State_Ptr = &A0;
	A_Task_Ptr = &A1;
	B_Task_Ptr = &B1;
	C_Task_Ptr = &C1;

// Initialize PWM module
    pwm1.PeriodMax = SYSTEM_FREQUENCY*1000000*T/2;  // Prescaler X1 (T1), ISR period = T x 1 
	PWM_INIT_MACRO(pwm1)  

// Initialize PWMDAC module
	pwmdac1.PeriodMax = 500;   // @60Mhz: 1500->20kHz, 1000-> 30kHz, 500->60kHz
    pwmdac1.PwmDacInPointer0 = &PwmDacCh1;
    pwmdac1.PwmDacInPointer1 = &PwmDacCh2;
    pwmdac1.PwmDacInPointer2 = &PwmDacCh3;
    pwmdac1.PwmDacInPointer3 = &PwmDacCh4;

	PWMDAC_INIT_MACRO(pwmdac1)

// Initialize DATALOG module
    dlog.iptr1 = &DlogCh1;
    dlog.iptr2 = &DlogCh2;
	dlog.iptr3 = &DlogCh3;
    dlog.iptr4 = &DlogCh4;
    dlog.trig_value = 0x1;
    dlog.size = 0x00c8;
    dlog.prescalar = 5;
    dlog.init(&dlog);

// Initialize ADC module
    ADC_MACRO()

// Initialize QEP module
    qep1.LineEncoder = 2048;
    qep1.MechScaler = _IQ30(0.25/qep1.LineEncoder);
    qep1.PolePairs = POLES/2;
    qep1.CalibratedAngle = 0;
    QEP_INIT_MACRO(qep1)

// Initialize the Speed module for QEP based speed calculation
    speed1.K1 = _IQ21(1/(BASE_FREQ*T));
    speed1.K2 = _IQ(1/(1+T*2*PI*5));  // Low-pass cut-off frequency
    speed1.K3 = _IQ(1)-speed1.K2;
    speed1.BaseRpm = 120*(BASE_FREQ/POLES);

// Initialize the SPEED_EST module SMOPOS based speed calculation
    speed3.K1 = _IQ21(1/(BASE_FREQ*T));
    speed3.K2 = _IQ(1/(1+T*2*PI*5));  // Low-pass cut-off frequency 
    speed3.K3 = _IQ(1)-speed3.K2;
    speed3.BaseRpm = 120*(BASE_FREQ/POLES);

// Initialize the RAMPGEN module
    rg1.StepAngleMax = _IQ(BASE_FREQ*T);

// Initialize the SMOPOS constant module
	smo1_const.Rs = RS;
	smo1_const.Ls = LS;
	smo1_const.Ib = BASE_CURRENT;
	smo1_const.Vb = BASE_VOLTAGE;
	smo1_const.Ts = T;
	SMO_CONST_MACRO(smo1_const)

// Initialize the SMOPOS module
 	smo1.Fsmopos = _IQ(smo1_const.Fsmopos);
 	smo1.Gsmopos = _IQ(smo1_const.Gsmopos); 
 	smo1.Kslide = _IQ(0.15);
    smo1.Kslf = _IQ(0.10);   
     	
// Initialize the PID_GRANDO_CONTROLLER module for Id 
	pid1_id.param.Kp = _IQ(3.176*BASE_CURRENT/BASE_VOLTAGE);	//Anaheim
//	pid1_id.param.Kp = _IQ(0.25*BASE_CURRENT/BASE_VOLTAGE);		//test motor 24V
//	pid1_id.param.Kp = _IQ(0.125*BASE_CURRENT/BASE_VOLTAGE);		//test motor 48V
    pid1_id.param.Kr = _IQ(1.0);
	pid1_id.param.Ki = _IQ(T/0.0005);							//Anaheim
//	pid1_id.param.Ki = _IQ(T/0.0956);							//test motor
	pid1_id.param.Kd = _IQ(0/T);
    pid1_id.param.Km = _IQ(1.0);
    pid1_id.param.Umax = _IQ(0.50);
    pid1_id.param.Umin = _IQ(-0.50);
 
// Initialize the PID_GRANDO_CONTROLLER module for Iq 
	pid1_iq.param.Kp = _IQ(3.176*BASE_CURRENT/BASE_VOLTAGE);	//Anaheim
//	pid1_iq.param.Kp = _IQ(0.25*BASE_CURRENT/BASE_VOLTAGE);		//test motor 24V
//	pid1_iq.param.Kp = _IQ(0.125*BASE_CURRENT/BASE_VOLTAGE);		//test motor 48V
    pid1_iq.param.Kr = _IQ(1.0);
	pid1_iq.param.Ki = _IQ(T/0.0005);							//Anaheim
//	pid1_iq.param.Ki = _IQ(T/0.0956);							//test motor
	pid1_iq.param.Kd = _IQ(0/T);
    pid1_iq.param.Km = _IQ(1.0);
    pid1_iq.param.Umax = _IQ(0.95);
    pid1_iq.param.Umin = _IQ(-0.95);
 
// Initialize the PID_GRANDO_CONTROLLER module for Speed 
    pid1_spd.param.Kp = _IQ(0.2*BASE_FREQ/BASE_CURRENT/(POLES/2));	//Anaheim
//    pid1_spd.param.Kp = _IQ(1.0*BASE_FREQ/BASE_CURRENT/(POLES/2));		//test motor
    pid1_spd.param.Kr = _IQ(1.0);
	pid1_spd.param.Ki = _IQ(T*SpeedLoopPrescaler/0.3);				//Anaheim
//	pid1_spd.param.Ki = _IQ(T*SpeedLoopPrescaler/0.6);				//test motor
	pid1_spd.param.Kd = _IQ(0/(T*SpeedLoopPrescaler));
    pid1_spd.param.Km = _IQ(1.0);
    pid1_spd.param.Umax = _IQ(0.95);
    pid1_spd.param.Umin = _IQ(-0.95);
 
// Initialize the phase current offset calibration filter
	cal_filt_gain = _IQ15(T/(T+TC_CAL));
	
#ifdef DRV8301
// Initialize SPI for communication to the DRV8301
	DRV8301_SPI_Init(&SpibRegs);
#endif
	
// Reassign ISRs. 

	EALLOW;	// This is needed to write to EALLOW protected registers
	PieVectTable.EPWM1_INT = &MainISR;
	EDIS;

// Enable PIE group 3 interrupt 1 for EPWM1_INT
    PieCtrlRegs.PIEIER3.bit.INTx1 = 1;

// Enable CNT_zero interrupt using EPWM1 Time-base
    EPwm1Regs.ETSEL.bit.INTEN = 1;   // Enable EPWM1INT generation 
    EPwm1Regs.ETSEL.bit.INTSEL = 1;  // Enable interrupt CNT_zero event
    EPwm1Regs.ETPS.bit.INTPRD = 1;   // Generate interrupt on the 1st event
	EPwm1Regs.ETCLR.bit.INT = 1;     // Enable more interrupts

// Enable CPU INT3 for EPWM1_INT:
	IER |= M_INT3;
// Enable global Interrupts and higher priority real-time debug events:
	EINT;   // Enable Global interrupt INTM
	ERTM;	// Enable Global realtime interrupt DBGM

// IDLE loop. Just sit and loop forever:	
	for(;;)  //infinite loop
	{
		// State machine entry & exit point
		//===========================================================
		(*Alpha_State_Ptr)();	// jump to an Alpha state (A0,B0,...)
		//===========================================================

#ifdef DRV8301
		//read the status registers from the DRV8301
		if(read_drv_status)
		{
			if(GpioDataRegs.GPADAT.bit.GPIO14 == 0)
			{
				DRV8301_stat_reg1.all = DRV8301_SPI_Read(&SpibRegs,STAT_REG_1_ADDR);
				DRV8301_stat_reg2.all = DRV8301_SPI_Read(&SpibRegs,STAT_REG_2_ADDR); 
				read_drv_status = 0;
			}
		}
#endif		
	}
} //END MAIN CODE



//=================================================================================
//	STATE-MACHINE SEQUENCING AND SYNCRONIZATION FOR SLOW BACKGROUND TASKS
//=================================================================================

//--------------------------------- FRAMEWORK -------------------------------------
void A0(void)
{
	// loop rate synchronizer for A-tasks
	if(CpuTimer0Regs.TCR.bit.TIF == 1)
	{
		CpuTimer0Regs.TCR.bit.TIF = 1;	// clear flag

		//-----------------------------------------------------------
		(*A_Task_Ptr)();		// jump to an A Task (A1,A2,A3,...)
		//-----------------------------------------------------------

		VTimer0[0]++;			// virtual timer 0, instance 0 (spare)
		SerialCommsTimer++;
	}

	Alpha_State_Ptr = &B0;		// Comment out to allow only A tasks
}

void B0(void)
{
	// loop rate synchronizer for B-tasks
	if(CpuTimer1Regs.TCR.bit.TIF == 1)
	{
		CpuTimer1Regs.TCR.bit.TIF = 1;				// clear flag

		//-----------------------------------------------------------
		(*B_Task_Ptr)();		// jump to a B Task (B1,B2,B3,...)
		//-----------------------------------------------------------
		VTimer1[0]++;			// virtual timer 1, instance 0 (spare)
	}

	Alpha_State_Ptr = &C0;		// Allow C state tasks
}

void C0(void)
{
	// loop rate synchronizer for C-tasks
	if(CpuTimer2Regs.TCR.bit.TIF == 1)
	{
		CpuTimer2Regs.TCR.bit.TIF = 1;				// clear flag

		//-----------------------------------------------------------
		(*C_Task_Ptr)();		// jump to a C Task (C1,C2,C3,...)
		//-----------------------------------------------------------
		VTimer2[0]++;			//virtual timer 2, instance 0 (spare)
	}

	Alpha_State_Ptr = &A0;	// Back to State A0
}


//=================================================================================
//	A - TASKS (executed in every 1 msec)
//=================================================================================
//--------------------------------------------------------
void A1(void) // SPARE (not used)
//--------------------------------------------------------
{
	if (EnableFlag == FALSE)
	{
		//de-assert the DRV830x EN_GATE pin
		#ifdef DSP2803x_DEVICE_H
		GpioDataRegs.GPBCLEAR.bit.GPIO39 = 1;
		#endif
		#ifdef F2806x_DEVICE_H
		GpioDataRegs.GPBCLEAR.bit.GPIO51 = 1;
		#endif

		RunMotor = FALSE;
		
		EALLOW;
	 	EPwm1Regs.TZFRC.bit.OST=1;
		EPwm2Regs.TZFRC.bit.OST=1;
		EPwm3Regs.TZFRC.bit.OST=1;
	 	EDIS;
	}
	else if((EnableFlag == TRUE) && (RunMotor == FALSE))
	{

#ifdef DRV8302
#if DRV_GAIN == 10
		GpioDataRegs.GPACLEAR.bit.GPIO25 = 1;	// GAIN = 10
#elif DRV_GAIN == 40
		GpioDataRegs.GPASET.bit.GPIO25 = 1;		// GAIN = 40
#else
#error  Invalid GAIN setting for DRV8302!!
#endif
		//GpioDataRegs.GPACLEAR.bit.GPIO24 = 1;	// M_OC - cycle by cycle current limit
		GpioDataRegs.GPASET.bit.GPIO24 = 1;		// M_OC - fault on OC
#endif

		//if we want the power stage active we need to enable the DRV830x
		//and configure it.
		if(DRV_RESET == 0)
		{
			//assert the DRV830x EN_GATE pin
			#ifdef DSP2803x_DEVICE_H
			GpioDataRegs.GPBSET.bit.GPIO39 = 1;
			#endif
			#ifdef F2806x_DEVICE_H
			GpioDataRegs.GPBSET.bit.GPIO51 = 1;
			#endif

			DELAY_US(50000);		//delay to allow DRV830x supplies to ramp up
			
#ifdef DRV8301
			DRV8301_cntrl_reg1.bit.GATE_CURRENT = 0;		// full current 1.7A
//			DRV8301_cntrl_reg1.bit.GATE_CURRENT = 1;		// med current 0.7A
//			DRV8301_cntrl_reg1.bit.GATE_CURRENT = 2;		// min current 0.25A
			DRV8301_cntrl_reg1.bit.GATE_RESET = 0;			// Normal Mode
			DRV8301_cntrl_reg1.bit.PWM_MODE = 0;			// six independant PWMs
//			DRV8301_cntrl_reg1.bit.OC_MODE = 0;				// current limiting when OC detected
			DRV8301_cntrl_reg1.bit.OC_MODE = 1;				// latched OC shutdown
//			DRV8301_cntrl_reg1.bit.OC_MODE = 2;				// Report on OCTWn pin and SPI reg only, no shut-down
//			DRV8301_cntrl_reg1.bit.OC_MODE = 3;				// OC protection disabled
//			DRV8301_cntrl_reg1.bit.OC_ADJ_SET = 0;			// OC @ Vds=0.060V
//			DRV8301_cntrl_reg1.bit.OC_ADJ_SET = 4;			// OC @ Vds=0.097V
//			DRV8301_cntrl_reg1.bit.OC_ADJ_SET = 6;			// OC @ Vds=0.123V
//			DRV8301_cntrl_reg1.bit.OC_ADJ_SET = 9;			// OC @ Vds=0.175V
			DRV8301_cntrl_reg1.bit.OC_ADJ_SET = 15;			// OC @ Vds=0.358V
//			DRV8301_cntrl_reg1.bit.OC_ADJ_SET = 16;			// OC @ Vds=0.403V
//			DRV8301_cntrl_reg1.bit.OC_ADJ_SET = 17;			// OC @ Vds=0.454V
//			DRV8301_cntrl_reg1.bit.OC_ADJ_SET = 18;			// OC @ Vds=0.511V
			DRV8301_cntrl_reg1.bit.Reserved = 0;
			
//			DRV8301_cntrl_reg2.bit.OCTW_SET = 0;			// report OT and OC
			DRV8301_cntrl_reg2.bit.OCTW_SET = 1;			// report OT only
#if DRV_GAIN == 10
			DRV8301_cntrl_reg2.bit.GAIN = 0;				// CS amplifier gain = 10
#elif DRV_GAIN == 20
			DRV8301_cntrl_reg2.bit.GAIN = 1;				// CS amplifier gain = 20
#elif DRV_GAIN == 40
			DRV8301_cntrl_reg2.bit.GAIN = 2;				// CS amplifier gain = 40
#elif DRV_GAIN == 80
			DRV8301_cntrl_reg2.bit.GAIN = 3;				// CS amplifier gain = 80
#endif
			DRV8301_cntrl_reg2.bit.DC_CAL_CH1 = 0;			// not in CS calibrate mode
			DRV8301_cntrl_reg2.bit.DC_CAL_CH2 = 0;			// not in CS calibrate mode
			DRV8301_cntrl_reg2.bit.OC_TOFF = 0;				// normal mode
			DRV8301_cntrl_reg2.bit.Reserved = 0;
			
			//write to DRV8301 control register 1, returns status register 1 
			DRV8301_stat_reg1.all = DRV8301_SPI_Write(&SpibRegs,CNTRL_REG_1_ADDR,DRV8301_cntrl_reg1.all);
			//write to DRV8301 control register 2, returns status register 1
			DRV8301_stat_reg1.all = DRV8301_SPI_Write(&SpibRegs,CNTRL_REG_2_ADDR,DRV8301_cntrl_reg2.all);

#endif
		}

		speed3.EstimatedSpeed=0;
		speed3.EstimatedTheta=0;
		speed3.OldEstimatedTheta=0;
		speed3.EstimatedSpeedRpm=0;

		rg1.Freq=0;
		rg1.Out=0;
		rg1.Angle=0;
		rc1.TargetValue=0;
		rc1.SetpointValue=0;

		smo1.Theta=0;
		smo1.Ealpha=0;
		smo1.Ebeta=0;

		pid1_id.data.d1 = 0;
		pid1_id.data.d2 = 0;
		pid1_id.data.i1 = 0;
		pid1_id.data.ud = 0;
		pid1_id.data.ui = 0;
		pid1_id.data.up = 0;
		pid1_id.data.v1 = 0;
		pid1_id.data.w1 = 0;
		pid1_id.term.Out = 0;

		pid1_iq.data.d1 = 0;
		pid1_iq.data.d2 = 0;
		pid1_iq.data.i1 = 0;
		pid1_iq.data.ud = 0;
		pid1_iq.data.ui = 0;
		pid1_iq.data.up = 0;
		pid1_iq.data.v1 = 0;
		pid1_iq.data.w1 = 0;
		pid1_iq.term.Out = 0;

		pid1_spd.data.d1 = 0;
		pid1_spd.data.d2 = 0;
		pid1_spd.data.i1 = 0;
		pid1_spd.data.ud = 0;
		pid1_spd.data.ui = 0;
		pid1_spd.data.up = 0;
		pid1_spd.data.v1 = 0;
		pid1_spd.data.w1 = 0;
		pid1_spd.term.Out = 0;

		lsw=0; 

		RunMotor = TRUE;
			
		EALLOW;
		EPwm1Regs.TZCLR.bit.OST=1;
		EPwm2Regs.TZCLR.bit.OST=1;
		EPwm3Regs.TZCLR.bit.OST=1;
		EDIS;

		
	}
	//-------------------
	//the next time CpuTimer0 'counter' reaches Period value go to A2
	A_Task_Ptr = &A2;
	//-------------------
}

//-----------------------------------------------------------------
void A2(void) // SPARE (not used)
//-----------------------------------------------------------------
{	

	//-------------------
	//the next time CpuTimer0 'counter' reaches Period value go to A3
	A_Task_Ptr = &A3;
	//-------------------
}

//-----------------------------------------
void A3(void) // SPARE (not used)
//-----------------------------------------
{

	//-----------------
	//the next time CpuTimer0 'counter' reaches Period value go to A1
	A_Task_Ptr = &A1;
	//-----------------
}



//=================================================================================
//	B - TASKS (executed in every 5 msec)
//=================================================================================

//----------------------------------- USER ----------------------------------------

//----------------------------------------
void B1(void) // Toggle GPIO-00
//----------------------------------------
{

	//-----------------
	//the next time CpuTimer1 'counter' reaches Period value go to B2
	B_Task_Ptr = &B2;	
	//-----------------
}

//----------------------------------------
void B2(void) //  SPARE
//----------------------------------------
{

	//-----------------
	//the next time CpuTimer1 'counter' reaches Period value go to B3
	B_Task_Ptr = &B3;
	//-----------------
}

//----------------------------------------
void B3(void) //  SPARE
//----------------------------------------
{

	//-----------------
	//the next time CpuTimer1 'counter' reaches Period value go to B1
	B_Task_Ptr = &B1;	
	//-----------------
}


//=================================================================================
//	C - TASKS (executed in every 50 msec)
//=================================================================================

//--------------------------------- USER ------------------------------------------

//----------------------------------------
void C1(void) 	// Toggle GPIO-34 
//----------------------------------------
{

	GpioDataRegs.GPBTOGGLE.bit.GPIO34 = 1;	// Blink LED
	//-----------------
	//the next time CpuTimer2 'counter' reaches Period value go to C2
	C_Task_Ptr = &C2;	
	//-----------------

}

//----------------------------------------
void C2(void) //  SPARE
//----------------------------------------
{

	//-----------------
	//the next time CpuTimer2 'counter' reaches Period value go to C3
	C_Task_Ptr = &C3;	
	//-----------------
}


//-----------------------------------------
void C3(void) //  SPARE
//-----------------------------------------
{

	//-----------------
	//the next time CpuTimer2 'counter' reaches Period value go to C1
	C_Task_Ptr = &C1;	
	//-----------------
}




// MainISR 
interrupt void MainISR(void)
{

// Verifying the ISR
    IsrTicker++;

if(RunMotor)
	{
// =============================== LEVEL 1 ======================================
//	  Checks target independent modules, duty cycle waveforms and PWM update
//	  Keep the motors disconnected at this level	
// ============================================================================== 

#if (BUILDLEVEL==LEVEL1)	 

// ------------------------------------------------------------------------------
//  Connect inputs of the RMP module and call the ramp control macro
// ------------------------------------------------------------------------------
    rc1.TargetValue = SpeedRef;		
    RC_MACRO(rc1)

// ------------------------------------------------------------------------------
//  Connect inputs of the RAMP GEN module and call the ramp generator macro
// ------------------------------------------------------------------------------
    rg1.Freq = rc1.SetpointValue;
    RG_MACRO(rg1)

// ------------------------------------------------------------------------------
//  Connect inputs of the INV_PARK module and call the inverse park trans. macro
// ------------------------------------------------------------------------------
    ipark1.Ds = VdTesting;
    ipark1.Qs = VqTesting;

    ipark1.Sine=_IQsinPU(rg1.Out);
    ipark1.Cosine=_IQcosPU(rg1.Out);
    IPARK_MACRO(ipark1)

// ------------------------------------------------------------------------------
//  Connect inputs of the SVGEN_DQ module and call the space-vector gen. macro
// ------------------------------------------------------------------------------
    svgen_dq1.Ualpha = ipark1.Alpha;
    svgen_dq1.Ubeta = ipark1.Beta;	
    SVGEN_MACRO(svgen_dq1)

// ------------------------------------------------------------------------------
//  Connect inputs of the PWM_DRV module and call the PWM signal generation macro
// ------------------------------------------------------------------------------
    pwm1.MfuncC1 = _IQtoQ15(svgen_dq1.Ta);
    pwm1.MfuncC2 = _IQtoQ15(svgen_dq1.Tb);  
    pwm1.MfuncC3 = _IQtoQ15(svgen_dq1.Tc);
    PWM_MACRO(pwm1)							   	   // Calculate the new PWM compare values	


	EPwm1Regs.CMPA.half.CMPA=pwm1.PWM1out;	// PWM 1A - PhaseA 
	EPwm2Regs.CMPA.half.CMPA=pwm1.PWM2out;	// PWM 2A - PhaseB
	EPwm3Regs.CMPA.half.CMPA=pwm1.PWM3out;	// PWM 3A - PhaseC  



// ------------------------------------------------------------------------------
//    Connect inputs of the PWMDAC module 
// ------------------------------------------------------------------------------	
    PwmDacCh1 = _IQtoQ15(svgen_dq1.Ta);
    PwmDacCh2 = _IQtoQ15(svgen_dq1.Tb);
    PwmDacCh3 = _IQtoQ15(svgen_dq1.Tc);
    PwmDacCh4 = _IQtoQ15(svgen_dq1.Tb-svgen_dq1.Tc);

// ------------------------------------------------------------------------------
//    Connect inputs of the DATALOG module 
// ------------------------------------------------------------------------------
    DlogCh1 = _IQtoQ15(svgen_dq1.Ta);
    DlogCh2 = _IQtoQ15(svgen_dq1.Tb);
    DlogCh3 = _IQtoQ15(svgen_dq1.Tc);
    DlogCh4 = _IQtoQ15(svgen_dq1.Tb-svgen_dq1.Tc);

#endif // (BUILDLEVEL==LEVEL1)

// =============================== LEVEL 2 ======================================
//	  Level 2 verifies the analog-to-digital conversion, offset compensation, 
//    clarke/park transformations (CLARKE/PARK), phase voltage calculations 
// ============================================================================== 

#if (BUILDLEVEL==LEVEL2) 

// ------------------------------------------------------------------------------
//  Connect inputs of the RMP module and call the ramp control macro
// ------------------------------------------------------------------------------
    rc1.TargetValue = SpeedRef;		
    RC_MACRO(rc1)

// ------------------------------------------------------------------------------
//  Connect inputs of the RAMP GEN module and call the ramp generator macro
// ------------------------------------------------------------------------------
    rg1.Freq = rc1.SetpointValue;
    RG_MACRO(rg1) 

// ------------------------------------------------------------------------------
//  Measure phase currents, subtract the offset and normalize from (-0.5,+0.5) to (-1,+1). 
//	Connect inputs of the CLARKE module and call the clarke transformation macro
// ------------------------------------------------------------------------------
	#ifdef F2806x_DEVICE_H
//	clarke1.As=-(((AdcResult.ADCRESULT0)*0.00024414-cal_offset_A)*2); // Phase A curr.
//	clarke1.Bs=-(((AdcResult.ADCRESULT1)*0.00024414-cal_offset_B)*2); // Phase B curr.
	clarke1.As=(((AdcResult.ADCRESULT0)*0.00024414-cal_offset_A)*2); // Phase A curr.
	clarke1.Bs=(((AdcResult.ADCRESULT1)*0.00024414-cal_offset_B)*2); // Phase B curr.
	#endif													 	   // ((ADCmeas(q12)/2^12)-0.5)*2			

	#ifdef DSP2803x_DEVICE_H
//    clarke1.As=-(_IQ15toIQ((AdcResult.ADCRESULT1<<3)-cal_offset_A)<<1);
//    clarke1.Bs=-(_IQ15toIQ((AdcResult.ADCRESULT2<<3)-cal_offset_B)<<1);
    clarke1.As=(_IQ15toIQ((AdcResult.ADCRESULT1<<3)-cal_offset_A)<<1);
    clarke1.Bs=(_IQ15toIQ((AdcResult.ADCRESULT2<<3)-cal_offset_B)<<1);
    #endif

    CLARKE_MACRO(clarke1) 

// ------------------------------------------------------------------------------
//  Connect inputs of the PARK module and call the park trans. macro
// ------------------------------------------------------------------------------
    park1.Alpha = clarke1.Alpha;
    park1.Beta = clarke1.Beta;
    park1.Angle = rg1.Out;
    park1.Sine = _IQsinPU(park1.Angle);
    park1.Cosine = _IQcosPU(park1.Angle);
    PARK_MACRO(park1) 
 
// ------------------------------------------------------------------------------
//	Connect inputs of the INV_PARK module and call the inverse park trans. macro
// ------------------------------------------------------------------------------
    ipark1.Ds = VdTesting;
    ipark1.Qs = VqTesting;
    ipark1.Sine=park1.Sine;
    ipark1.Cosine=park1.Cosine;
    IPARK_MACRO(ipark1) 

// ------------------------------------------------------------------------------
//  Connect inputs of the VOLT_CALC module and call the phase voltage calc. macro
// ------------------------------------------------------------------------------
	#ifdef F2806x_DEVICE_H
	volt1.DcBusVolt = ((AdcResult.ADCRESULT2)*0.00024414); // DC Bus voltage meas.
    #endif														 // (ADCmeas(q12)/2^12)	
    
    #ifdef DSP2803x_DEVICE_H
	volt1.DcBusVolt = _IQ15toIQ((AdcResult.ADCRESULT3<<3));	     // DC Bus voltage meas.
    #endif

    volt1.MfuncV1 = svgen_dq1.Ta;
    volt1.MfuncV2 = svgen_dq1.Tb;
    volt1.MfuncV3 = svgen_dq1.Tc;
    VOLT_MACRO(volt1)        

// ------------------------------------------------------------------------------
//  Connect inputs of the SVGEN_DQ module and call the space-vector gen. macro
// ------------------------------------------------------------------------------
    svgen_dq1.Ualpha = ipark1.Alpha;
    svgen_dq1.Ubeta = ipark1.Beta;
    SVGEN_MACRO(svgen_dq1)	

// ------------------------------------------------------------------------------
//  Connect inputs of the PWM_DRV module and call the PWM signal generation macro
// ------------------------------------------------------------------------------
    pwm1.MfuncC1 = _IQtoQ15(svgen_dq1.Ta);
    pwm1.MfuncC2 = _IQtoQ15(svgen_dq1.Tb);  
    pwm1.MfuncC3 = _IQtoQ15(svgen_dq1.Tc);
    PWM_MACRO(pwm1)							   	   // Calculate the new PWM compare values	


	EPwm1Regs.CMPA.half.CMPA=pwm1.PWM1out;	// PWM 1A - PhaseA 
	EPwm2Regs.CMPA.half.CMPA=pwm1.PWM2out;	// PWM 2A - PhaseB
	EPwm3Regs.CMPA.half.CMPA=pwm1.PWM3out;	// PWM 3A - PhaseC
 
// ------------------------------------------------------------------------------
//  Connect inputs of the PWCCMDAC module 
// ------------------------------------------------------------------------------	
    PwmDacCh1 = _IQtoQ15(volt1.Valpha); 
    PwmDacCh2 = _IQtoQ15(clarke1.Alpha);
    PwmDacCh3 = _IQtoQ15(volt1.Vbeta );
    PwmDacCh4 = _IQtoQ15(clarke1.Beta); 

// ------------------------------------------------------------------------------
//  Connect inputs of the DATALOG module 
// ------------------------------------------------------------------------------
    DlogCh1 = _IQtoQ15(volt1.Valpha);
    DlogCh2 = _IQtoQ15(clarke1.Alpha);
    DlogCh3 = _IQtoQ15(volt1.Vbeta );
    DlogCh4 = _IQtoQ15(clarke1.Beta);  


#endif // (BUILDLEVEL==LEVEL2)

// =============================== LEVEL 3 ======================================
//	  Level 3 auto-calculates the current sensor offset calibration 
// ============================================================================== 

#if (BUILDLEVEL==LEVEL3) 

_iq IAfdbk;
_iq IBfdbk;
// ------------------------------------------------------------------------------
//  Measure phase currents, subtract the offset and normalize from (-0.5,+0.5) to (-1,+1). 
// ------------------------------------------------------------------------------
	#ifdef F2806x_DEVICE_H
	IAfdbk=((AdcResult.ADCRESULT0)*0.00024414-cal_offset_A)*2; // Phase A curr.
	IBfdbk=((AdcResult.ADCRESULT1)*0.00024414-cal_offset_B)*2; // Phase B curr.
	#endif													 	   // ((ADCmeas(q12)/2^12)-0.5)*2			

	#ifdef DSP2803x_DEVICE_H
    IAfdbk=_IQ15toIQ((AdcResult.ADCRESULT1<<3)-cal_offset_A)<<1;
    IBfdbk=_IQ15toIQ((AdcResult.ADCRESULT2<<3)-cal_offset_B)<<1;
    #endif
    
// ------------------------------------------------------------------------------
//  LPF to average the calibration offsets
//  Use the offsets calculated here to initialize cal_offset_A and cal_offset_B
//  so that they are used for the remaining build levels
// ------------------------------------------------------------------------------
    cal_offset_A = _IQ15mpy(cal_filt_gain,_IQtoIQ15(IAfdbk)) + cal_offset_A;
    cal_offset_B = _IQ15mpy(cal_filt_gain,_IQtoIQ15(IBfdbk)) + cal_offset_B;

// ------------------------------------------------------------------------------
//  force all PWMs to 0% duty cycle
// ------------------------------------------------------------------------------
	EPwm1Regs.CMPA.half.CMPA=pwm1.PeriodMax;	// PWM 1A - PhaseA 
	EPwm2Regs.CMPA.half.CMPA=pwm1.PeriodMax;	// PWM 2A - PhaseB
	EPwm3Regs.CMPA.half.CMPA=pwm1.PeriodMax;	// PWM 3A - PhaseC  

// ------------------------------------------------------------------------------
//  Connect inputs of the PWMDAC module 
// ------------------------------------------------------------------------------	
    PwmDacCh1 = _IQtoQ15(IAfdbk);
    PwmDacCh2 = _IQtoQ15(IBfdbk);
    PwmDacCh2 = _IQtoQ15(cal_offset_A); 
    PwmDacCh3 = _IQtoQ15(cal_offset_B); 

// ------------------------------------------------------------------------------
//  Connect inputs of the DATALOG module 
// ------------------------------------------------------------------------------
    DlogCh1 = _IQtoQ15(IAfdbk);
    DlogCh2 = _IQtoQ15(IBfdbk);
    DlogCh3 = _IQtoQ15(cal_offset_A);
    DlogCh4 = _IQtoQ15(cal_offset_B);  
#endif // (BUILDLEVEL==LEVEL3)

// =============================== LEVEL 4 ======================================
//	Level 4 verifies the dq-axis current regulation performed by PID and speed 
//	measurement modules  
// ==============================================================================  
//  lsw=0: lock the rotor of the motor 
//  lsw=1: close the current loop


#if (BUILDLEVEL==LEVEL4)

// ------------------------------------------------------------------------------
//  Connect inputs of the RMP module and call the ramp control macro
// ------------------------------------------------------------------------------ 
    if(lsw==0) rc1.TargetValue = 0;
    else rc1.TargetValue = SpeedRef;		
    RC_MACRO(rc1)

// ------------------------------------------------------------------------------
//  Connect inputs of the RAMP GEN module and call the ramp generator macro
// ------------------------------------------------------------------------------
    rg1.Freq = rc1.SetpointValue;
    RG_MACRO(rg1)  

// ------------------------------------------------------------------------------
//  Measure phase currents, subtract the offset and normalize from (-0.5,+0.5) to (-1,+1). 
//	Connect inputs of the CLARKE module and call the clarke transformation macro
// ------------------------------------------------------------------------------  
	#ifdef F2806x_DEVICE_H
//	clarke1.As=-(((AdcResult.ADCRESULT0)*0.00024414-cal_offset_A)*2); // Phase A curr.
//	clarke1.Bs=-(((AdcResult.ADCRESULT1)*0.00024414-cal_offset_B)*2); // Phase B curr.
	clarke1.As=(((AdcResult.ADCRESULT0)*0.00024414-cal_offset_A)*2); // Phase A curr.
	clarke1.Bs=(((AdcResult.ADCRESULT1)*0.00024414-cal_offset_B)*2); // Phase B curr.
	#endif													 	   // ((ADCmeas(q12)/2^12)-0.5)*2			

	#ifdef DSP2803x_DEVICE_H
//    clarke1.As=-(_IQ15toIQ((AdcResult.ADCRESULT1<<3)-cal_offset_A)<<1);
//    clarke1.Bs=-(_IQ15toIQ((AdcResult.ADCRESULT2<<3)-cal_offset_B)<<1);
    clarke1.As=(_IQ15toIQ((AdcResult.ADCRESULT1<<3)-cal_offset_A)<<1);
    clarke1.Bs=(_IQ15toIQ((AdcResult.ADCRESULT2<<3)-cal_offset_B)<<1);
    #endif
  
    CLARKE_MACRO(clarke1) 

// ------------------------------------------------------------------------------
//  Connect inputs of the PARK module and call the park trans. macro
// ------------------------------------------------------------------------------ 
    park1.Alpha = clarke1.Alpha;
    park1.Beta = clarke1.Beta;
    if(lsw==0) park1.Angle = 0;
    else if(lsw==1) park1.Angle = rg1.Out;
	
    park1.Sine = _IQsinPU(park1.Angle);
    park1.Cosine = _IQcosPU(park1.Angle);
	
    PARK_MACRO(park1) 
 
// ------------------------------------------------------------------------------
//  Connect inputs of the PID_GRANDO_CONTROLLER module and call the PID IQ controller macro
// ------------------------------------------------------------------------------  
    if(lsw==0) pid1_iq.term.Ref = 0;
    else if(lsw==1) pid1_iq.term.Ref = IqRef;
    pid1_iq.term.Fbk = park1.Qs;
    PID_GR_MACRO(pid1_iq)

// ------------------------------------------------------------------------------
//  Connect inputs of the PID_GRANDO_CONTROLLER module and call the PID ID controller macro
// ------------------------------------------------------------------------------   
    if(lsw==0) pid1_id.term.Ref = _IQ(0.05);
    else pid1_id.term.Ref = IdRef; 
    pid1_id.term.Fbk = park1.Ds;
    PID_GR_MACRO(pid1_id) 

// ------------------------------------------------------------------------------
//	Connect inputs of the INV_PARK module and call the inverse park trans. macro
// ------------------------------------------------------------------------------
    ipark1.Ds = pid1_id.term.Out;
    ipark1.Qs = pid1_iq.term.Out ;
    ipark1.Sine   = park1.Sine;
    ipark1.Cosine = park1.Cosine;
    IPARK_MACRO(ipark1) 

// ------------------------------------------------------------------------------
//    Call the QEP calculation module 
// ------------------------------------------------------------------------------
    QEP_MACRO(qep1);

// ------------------------------------------------------------------------------
//    Connect inputs of the SPEED_FR module and call the speed calculation macro 
// ------------------------------------------------------------------------------ 
    speed1.ElecTheta = qep1.ElecTheta;
    speed1.DirectionQep = (int32)(qep1.DirectionQep);
    SPEED_FR_MACRO(speed1)

// ------------------------------------------------------------------------------
//  Connect inputs of the VOLT_CALC module and call the phase voltage calc. macro
// ------------------------------------------------------------------------------
	#ifdef F2806x_DEVICE_H
	volt1.DcBusVolt = ((AdcResult.ADCRESULT2)*0.00024414); // DC Bus voltage meas.
    #endif														 // (ADCmeas(q12)/2^12)	
    
    #ifdef DSP2803x_DEVICE_H
	volt1.DcBusVolt = _IQ15toIQ((AdcResult.ADCRESULT3<<3));	     // DC Bus voltage meas.
    #endif

    volt1.MfuncV1 = svgen_dq1.Ta;
    volt1.MfuncV2 = svgen_dq1.Tb;
    volt1.MfuncV3 = svgen_dq1.Tc;
    VOLT_MACRO(volt1)        

// ------------------------------------------------------------------------------
//  Connect inputs of the SVGEN_DQ module and call the space-vector gen. macro
// ------------------------------------------------------------------------------
    svgen_dq1.Ualpha = ipark1.Alpha;
    svgen_dq1.Ubeta = ipark1.Beta;
    SVGEN_MACRO(svgen_dq1)        

// ------------------------------------------------------------------------------
//  Connect inputs of the PWM_DRV module and call the PWM signal generation macro
// ------------------------------------------------------------------------------
    pwm1.MfuncC1 = _IQtoQ15(svgen_dq1.Ta);
    pwm1.MfuncC2 = _IQtoQ15(svgen_dq1.Tb);  
    pwm1.MfuncC3 = _IQtoQ15(svgen_dq1.Tc);
    PWM_MACRO(pwm1)							   	   // Calculate the new PWM compare values	


	EPwm1Regs.CMPA.half.CMPA=pwm1.PWM1out;	// PWM 1A - PhaseA 
	EPwm2Regs.CMPA.half.CMPA=pwm1.PWM2out;	// PWM 2A - PhaseB
	EPwm3Regs.CMPA.half.CMPA=pwm1.PWM3out;	// PWM 3A - PhaseC  

// ------------------------------------------------------------------------------
//    Connect inputs of the PWMDAC module 
// ------------------------------------------------------------------------------	
    PwmDacCh1 = _IQtoQ15(rg1.Out);
    PwmDacCh2 = _IQtoQ15(speed1.ElecTheta);    
    PwmDacCh3 = _IQtoQ15(clarke1.As);    
    PwmDacCh4 = _IQtoQ15(clarke1.Bs);  

// ------------------------------------------------------------------------------
//    Connect inputs of the DATALOG module 
// ------------------------------------------------------------------------------
    DlogCh1 = _IQtoQ15(rg1.Out);
    DlogCh2 = _IQtoQ15(svgen_dq1.Ta);
    DlogCh3 = _IQtoQ15(clarke1.As);
    DlogCh4 = _IQtoQ15(clarke1.Bs);  

#endif // (BUILDLEVEL==LEVEL4)


// =============================== LEVEL 5 ======================================
//	  Level 5 verifies the estimated rotor position and speed estimation 
//	  performed by SMOPOS and SPEED_EST modules, respectively. 
// ==============================================================================  
//  lsw=0: lock the rotor of the motor 
//  lsw=1: close the current loop

#if (BUILDLEVEL==LEVEL5)

// ------------------------------------------------------------------------------
//  Connect inputs of the RMP module and call the ramp control macro
// ------------------------------------------------------------------------------ 
    if(lsw==0)rc1.TargetValue = 0;
    else rc1.TargetValue = SpeedRef;		
    RC_MACRO(rc1)

// ------------------------------------------------------------------------------
//  Connect inputs of the RAMP GEN module and call the ramp generator macro
// ------------------------------------------------------------------------------
    rg1.Freq = rc1.SetpointValue;
    RG_MACRO(rg1)  

// ------------------------------------------------------------------------------
//  Measure phase currents, subtract the offset and normalize from (-0.5,+0.5) to (-1,+1). 
//	Connect inputs of the CLARKE module and call the clarke transformation macro
// ------------------------------------------------------------------------------
	#ifdef F2806x_DEVICE_H
//	clarke1.As=-(((AdcResult.ADCRESULT0)*0.00024414-cal_offset_A)*2); // Phase A curr.
//	clarke1.Bs=-(((AdcResult.ADCRESULT1)*0.00024414-cal_offset_B)*2); // Phase B curr.
	clarke1.As=(((AdcResult.ADCRESULT0)*0.00024414-cal_offset_A)*2); // Phase A curr.
	clarke1.Bs=(((AdcResult.ADCRESULT1)*0.00024414-cal_offset_B)*2); // Phase B curr.
	#endif													 	   // ((ADCmeas(q12)/2^12)-0.5)*2			

	#ifdef DSP2803x_DEVICE_H
//    clarke1.As=-(_IQ15toIQ((AdcResult.ADCRESULT1<<3)-cal_offset_A)<<1);
//    clarke1.Bs=-(_IQ15toIQ((AdcResult.ADCRESULT2<<3)-cal_offset_B)<<1);
    clarke1.As=(_IQ15toIQ((AdcResult.ADCRESULT1<<3)-cal_offset_A)<<1);
    clarke1.Bs=(_IQ15toIQ((AdcResult.ADCRESULT2<<3)-cal_offset_B)<<1);
    #endif

    CLARKE_MACRO(clarke1) 

// ------------------------------------------------------------------------------
//  Connect inputs of the PARK module and call the park trans. macro
// ------------------------------------------------------------------------------ 
    park1.Alpha = clarke1.Alpha;
    park1.Beta = clarke1.Beta;
    if(lsw==0) park1.Angle = 0;
    else if(lsw==1) park1.Angle = rg1.Out;
	
    park1.Sine = _IQsinPU(park1.Angle);
    park1.Cosine = _IQcosPU(park1.Angle);
	
    PARK_MACRO(park1) 
 
// ------------------------------------------------------------------------------
//  Connect inputs of the PID_GRANDO_CONTROLLER module and call the PID IQ controller macro
// ------------------------------------------------------------------------------  
    if(lsw==0) pid1_iq.term.Ref = 0;
    else if(lsw==1) pid1_iq.term.Ref = IqRef;
    pid1_iq.term.Fbk = park1.Qs;
    PID_GR_MACRO(pid1_iq)

// ------------------------------------------------------------------------------
//  Connect inputs of the PID_GRANDO_CONTROLLER module and call the PID ID controller macro
// ------------------------------------------------------------------------------   
    if(lsw==0) pid1_id.term.Ref = _IQ(0.05);
    else pid1_id.term.Ref = 0; 
    pid1_id.term.Fbk = park1.Ds;
    PID_GR_MACRO(pid1_id)

// ------------------------------------------------------------------------------
//	Connect inputs of the INV_PARK module and call the inverse park trans. macro
// ------------------------------------------------------------------------------ 
    ipark1.Ds = pid1_id.term.Out;
    ipark1.Qs = pid1_iq.term.Out;
    ipark1.Sine=park1.Sine;
    ipark1.Cosine=park1.Cosine;
    IPARK_MACRO(ipark1) 

// ------------------------------------------------------------------------------
//    Call the QEP calculation module 
// ------------------------------------------------------------------------------
    QEP_MACRO(qep1);

// ------------------------------------------------------------------------------
//    Connect inputs of the SPEED_FR module and call the speed calculation macro 
// ------------------------------------------------------------------------------
    speed1.ElecTheta = qep1.ElecTheta;
    speed1.DirectionQep = (int32)(qep1.DirectionQep);
    SPEED_FR_MACRO(speed1)

// ------------------------------------------------------------------------------
//  Connect inputs of the VOLT_CALC module and call the phase voltage calc. macro
// ------------------------------------------------------------------------------ 
	#ifdef F2806x_DEVICE_H
	volt1.DcBusVolt = ((AdcResult.ADCRESULT2)*0.00024414); // DC Bus voltage meas.
    #endif														 // (ADCmeas(q12)/2^12)	
    
    #ifdef DSP2803x_DEVICE_H
	volt1.DcBusVolt = _IQ15toIQ((AdcResult.ADCRESULT3<<3));	     // DC Bus voltage meas.
    #endif

    volt1.MfuncV1 = svgen_dq1.Ta;
    volt1.MfuncV2 = svgen_dq1.Tb;
    volt1.MfuncV3 = svgen_dq1.Tc;        
    VOLT_MACRO(volt1)

// ------------------------------------------------------------------------------
//    Connect inputs of the SMO_POS module and call the sliding-mode observer macro
// ------------------------------------------------------------------------------
    smo1.Ialpha = clarke1.Alpha;
  	smo1.Ibeta  = clarke1.Beta;
    smo1.Valpha = volt1.Valpha;
    smo1.Vbeta  = volt1.Vbeta;
    SMO_MACRO(smo1)

// ------------------------------------------------------------------------------
//    Connect inputs of the SPEED_EST module and call the estimated speed macro
// ------------------------------------------------------------------------------
    speed3.EstimatedTheta = smo1.Theta;
    SE_MACRO(speed3) 

// ------------------------------------------------------------------------------
//  Connect inputs of the SVGEN_DQ module and call the space-vector gen. macro
// ------------------------------------------------------------------------------
    svgen_dq1.Ualpha = ipark1.Alpha;
    svgen_dq1.Ubeta = ipark1.Beta;
    SVGEN_MACRO(svgen_dq1)        

// ------------------------------------------------------------------------------
//  Connect inputs of the PWM_DRV module and call the PWM signal generation macro
// ------------------------------------------------------------------------------
    pwm1.MfuncC1 = _IQtoQ15(svgen_dq1.Ta);
    pwm1.MfuncC2 = _IQtoQ15(svgen_dq1.Tb);  
    pwm1.MfuncC3 = _IQtoQ15(svgen_dq1.Tc);
    PWM_MACRO(pwm1)							   	   // Calculate the new PWM compare values	
    
	EPwm1Regs.CMPA.half.CMPA=pwm1.PWM1out;	// PWM 1A - PhaseA 
	EPwm2Regs.CMPA.half.CMPA=pwm1.PWM2out;	// PWM 2A - PhaseB
	EPwm3Regs.CMPA.half.CMPA=pwm1.PWM3out;	// PWM 3A - PhaseC  

// ------------------------------------------------------------------------------
//    Connect inputs of the PWMDAC module 
// ------------------------------------------------------------------------------	
    PwmDacCh1 = _IQtoQ15(smo1.Theta); 
    PwmDacCh2 = _IQtoQ15(rg1.Out); 
    PwmDacCh3 = _IQtoQ15(clarke1.As);
    PwmDacCh4 = _IQtoQ15(clarke1.Bs);

// ------------------------------------------------------------------------------
//    Connect inputs of the DATALOG module 
// ------------------------------------------------------------------------------
    DlogCh1 = _IQtoQ15(smo1.Theta);
    DlogCh2 = _IQtoQ15(rg1.Out);
    DlogCh3 = _IQtoQ15(clarke1.As);
    DlogCh4 = _IQtoQ15(clarke1.Bs);  


#endif // (BUILDLEVEL==LEVEL5)


// =============================== LEVEL 6 ======================================
//	  Level 6 verifies the speed regulator performed by PID_GRANDO_CONTROLLER module. 
//	  The system speed loop is closed by using the measured speed as a feedback.
// ==============================================================================  
//  lsw=0: lock the rotor of the motor 
//  lsw=1: close the current loop
//	lsw=2: close the speed/sensorless current loop


#if (BUILDLEVEL==LEVEL6)

// ------------------------------------------------------------------------------
//  Connect inputs of the RMP module and call the ramp control macro
// ------------------------------------------------------------------------------ 
    if(lsw==0)rc1.TargetValue = 0;
    else rc1.TargetValue = SpeedRef;		
    RC_MACRO(rc1)

// ------------------------------------------------------------------------------
//  Connect inputs of the RAMP GEN module and call the ramp generator macro
// ------------------------------------------------------------------------------
    rg1.Freq = rc1.SetpointValue;
    RG_MACRO(rg1)  

// ------------------------------------------------------------------------------
//  Measure phase currents, subtract the offset and normalize from (-0.5,+0.5) to (-1,+1). 
//	Connect inputs of the CLARKE module and call the clarke transformation macro
// ------------------------------------------------------------------------------
	#ifdef F2806x_DEVICE_H
//	clarke1.As=-(((AdcResult.ADCRESULT0)*0.00024414-cal_offset_A)*2); // Phase A curr.
//	clarke1.Bs=-(((AdcResult.ADCRESULT1)*0.00024414-cal_offset_B)*2); // Phase B curr.
	clarke1.As=(((AdcResult.ADCRESULT0)*0.00024414-cal_offset_A)*2); // Phase A curr.
	clarke1.Bs=(((AdcResult.ADCRESULT1)*0.00024414-cal_offset_B)*2); // Phase B curr.
	#endif													 	   // ((ADCmeas(q12)/2^12)-0.5)*2			

	#ifdef DSP2803x_DEVICE_H
//    clarke1.As=-(_IQ15toIQ((AdcResult.ADCRESULT1<<3)-cal_offset_A)<<1);
//    clarke1.Bs=-(_IQ15toIQ((AdcResult.ADCRESULT2<<3)-cal_offset_B)<<1);
    clarke1.As=(_IQ15toIQ((AdcResult.ADCRESULT1<<3)-cal_offset_A)<<1);
    clarke1.Bs=(_IQ15toIQ((AdcResult.ADCRESULT2<<3)-cal_offset_B)<<1);
    #endif
		
    CLARKE_MACRO(clarke1)  

// ------------------------------------------------------------------------------
//  Connect inputs of the PARK module and call the park trans. macro
// ------------------------------------------------------------------------------  
    park1.Alpha = clarke1.Alpha;
    park1.Beta = clarke1.Beta;
	
    if(lsw==0) park1.Angle = 0;
    else if(lsw==1) park1.Angle = rg1.Out;
    else park1.Angle = smo1.Theta;
	
    park1.Sine = _IQsinPU(park1.Angle);
    park1.Cosine = _IQcosPU(park1.Angle);
	
    PARK_MACRO(park1) 

// ------------------------------------------------------------------------------
//    Connect inputs of the PID_GRANDO_CONTROLLER module and call the PID speed controller macro
// ------------------------------------------------------------------------------  
  if (SpeedLoopCount==SpeedLoopPrescaler)
     {
      pid1_spd.term.Ref = rc1.SetpointValue;
      pid1_spd.term.Fbk = speed1.Speed;
      PID_GR_MACRO(pid1_spd)
      SpeedLoopCount=1;
     }
  else SpeedLoopCount++;   

  if(lsw==0 || lsw==1)
  {
  	pid1_spd.data.ui=0;
  	pid1_spd.data.i1=0;
  }

// ------------------------------------------------------------------------------
//    Connect inputs of the PID_GRANDO_CONTROLLER module and call the PID IQ controller macro
// ------------------------------------------------------------------------------  
	if(lsw==0) pid1_iq.term.Ref = 0;
    else if(lsw==1) pid1_iq.term.Ref = IqRef;
    else pid1_iq.term.Ref = IqRef; //pid1_spd.term.Out; (pid1_spd.term.Out->Level 6A, IqRef->Level 6B) 
	pid1_iq.term.Fbk = park1.Qs;
	PID_GR_MACRO(pid1_iq)

// ------------------------------------------------------------------------------
//    Connect inputs of the PID_GRANDO_CONTROLLER module and call the PID ID controller macro
// ------------------------------------------------------------------------------  
    if(lsw==0) pid1_id.term.Ref = _IQ(0.05);
    else pid1_id.term.Ref = 0; 
    pid1_id.term.Fbk = park1.Ds;
    PID_GR_MACRO(pid1_id)

// ------------------------------------------------------------------------------
//  Connect inputs of the INV_PARK module and call the inverse park trans. macro
// ------------------------------------------------------------------------------   
    ipark1.Ds = pid1_id.term.Out;
    ipark1.Qs = pid1_iq.term.Out;
    ipark1.Sine=park1.Sine;
    ipark1.Cosine=park1.Cosine;
    IPARK_MACRO(ipark1) 

// ------------------------------------------------------------------------------
//    Call the QEP calculation module 
// ------------------------------------------------------------------------------
    QEP_MACRO(qep1);

// ------------------------------------------------------------------------------
//    Connect inputs of the SPEED_FR module and call the speed calculation macro 
// ------------------------------------------------------------------------------
    speed1.ElecTheta = qep1.ElecTheta;
    speed1.DirectionQep = (int32)(qep1.DirectionQep);
    SPEED_FR_MACRO(speed1)

// ------------------------------------------------------------------------------
//    Connect inputs of the VOLT_CALC module and call the phase voltage macro
// ------------------------------------------------------------------------------
	#ifdef F2806x_DEVICE_H
	volt1.DcBusVolt = ((AdcResult.ADCRESULT2)*0.00024414); // DC Bus voltage meas.
    #endif														 // (ADCmeas(q12)/2^12)	
    
    #ifdef DSP2803x_DEVICE_H
	volt1.DcBusVolt = _IQ15toIQ((AdcResult.ADCRESULT3<<3));	     // DC Bus voltage meas.
    #endif

    volt1.MfuncV1 = svgen_dq1.Ta;
    volt1.MfuncV2 = svgen_dq1.Tb;
    volt1.MfuncV3 = svgen_dq1.Tc;        
    VOLT_MACRO(volt1)

// ------------------------------------------------------------------------------
//    Connect inputs of the SMO_POS module and call the sliding-mode observer macro
// ------------------------------------------------------------------------------
    smo1.Ialpha = clarke1.Alpha;
  	smo1.Ibeta  = clarke1.Beta;
    smo1.Valpha = volt1.Valpha;
    smo1.Vbeta  = volt1.Vbeta;
    SMO_MACRO(smo1)

// ------------------------------------------------------------------------------
//    Connect inputs of the SPEED_EST module and call the estimated speed macro
// ------------------------------------------------------------------------------
    speed3.EstimatedTheta = smo1.Theta;
    SE_MACRO(speed3)  

// ------------------------------------------------------------------------------
//  Connect inputs of the SVGEN_DQ module and call the space-vector gen. macro
// ------------------------------------------------------------------------------
  	svgen_dq1.Ualpha = ipark1.Alpha;
    svgen_dq1.Ubeta = ipark1.Beta;
  	SVGEN_MACRO(svgen_dq1)        

// ------------------------------------------------------------------------------
//  Connect inputs of the PWM_DRV module and call the PWM signal generation macro
// ------------------------------------------------------------------------------
    pwm1.MfuncC1 = _IQtoQ15(svgen_dq1.Ta);
    pwm1.MfuncC2 = _IQtoQ15(svgen_dq1.Tb);  
    pwm1.MfuncC3 = _IQtoQ15(svgen_dq1.Tc);
    PWM_MACRO(pwm1)							   	   // Calculate the new PWM compare values	

	EPwm1Regs.CMPA.half.CMPA=pwm1.PWM1out;	// PWM 1A - PhaseA 
	EPwm2Regs.CMPA.half.CMPA=pwm1.PWM2out;	// PWM 2A - PhaseB
	EPwm3Regs.CMPA.half.CMPA=pwm1.PWM3out;	// PWM 3A - PhaseC  


// ------------------------------------------------------------------------------
//    Connect inputs of the PWMDAC module 
// ------------------------------------------------------------------------------	
    PwmDacCh1 = _IQtoQ15(smo1.Theta); 
    PwmDacCh2 = _IQtoQ15(rg1.Out); 
    PwmDacCh3 = _IQtoQ15(clarke1.As);
    PwmDacCh4 = _IQtoQ15(clarke1.Bs);

// ------------------------------------------------------------------------------
//    Connect inputs of the DATALOG module 
// ------------------------------------------------------------------------------
    DlogCh1 = _IQtoQ15(smo1.Theta);
    DlogCh2 = _IQtoQ15(rg1.Out);
    DlogCh3 = _IQtoQ15(clarke1.As);
    DlogCh4 = _IQtoQ15(clarke1.Bs); 

#endif // (BUILDLEVEL==LEVEL6) 

// =============================== LEVEL 7 ======================================
//	  Level 7 verifies the speed regulator performed by PID_GRANDO_CONTROLLER module. 
//	  The system speed loop is closed by using the estimated speed as a feedback.
// ==============================================================================  
//  lsw=0: lock the rotor of the motor
//  lsw=1: close the current loop 
//  lsw=2: close the speed loop

#if (BUILDLEVEL==LEVEL7)

// ------------------------------------------------------------------------------
//  Connect inputs of the RMP module and call the ramp control macro
// ------------------------------------------------------------------------------ 
    if(lsw==0)rc1.TargetValue = 0;
    else rc1.TargetValue = SpeedRef;		
    RC_MACRO(rc1)

// ------------------------------------------------------------------------------
//  Connect inputs of the RAMP GEN module and call the ramp generator macro
// ------------------------------------------------------------------------------
    rg1.Freq = rc1.SetpointValue;
    RG_MACRO(rg1)  

// ------------------------------------------------------------------------------
//  Measure phase currents, subtract the offset and normalize from (-0.5,+0.5) to (-1,+1). 
//	Connect inputs of the CLARKE module and call the clarke transformation macro
// ------------------------------------------------------------------------------

	#ifdef F2806x_DEVICE_H
//	clarke1.As=-(((AdcResult.ADCRESULT0)*0.00024414-cal_offset_A)*2); // Phase A curr.
//	clarke1.Bs=-((AdcResult.ADCRESULT1)*0.00024414-cal_offset_B)*2; // Phase B curr.
	clarke1.As=(((AdcResult.ADCRESULT0)*0.00024414-cal_offset_A)*2); // Phase A curr.
	clarke1.Bs=((AdcResult.ADCRESULT1)*0.00024414-cal_offset_B)*2; // Phase B curr.
	#endif													 	   // ((ADCmeas(q12)/2^12)-0.5)*2			

	#ifdef DSP2803x_DEVICE_H
//    clarke1.As=-(_IQ15toIQ((AdcResult.ADCRESULT1<<3)-cal_offset_A)<<1);
//    clarke1.Bs=-(_IQ15toIQ((AdcResult.ADCRESULT2<<3)-cal_offset_B)<<1);
    clarke1.As=(_IQ15toIQ((AdcResult.ADCRESULT1<<3)-cal_offset_A)<<1);
    clarke1.Bs=(_IQ15toIQ((AdcResult.ADCRESULT2<<3)-cal_offset_B)<<1);
    #endif

	CLARKE_MACRO(clarke1)  

// ------------------------------------------------------------------------------
//  Connect inputs of the PARK module and call the park trans. macro
// ------------------------------------------------------------------------------  
	park1.Alpha = clarke1.Alpha;
	park1.Beta = clarke1.Beta;
	
	if(lsw==0) park1.Angle = 0;
	else if(lsw==1) park1.Angle = rg1.Out;
	else park1.Angle = smo1.Theta;
	
	park1.Sine = _IQsinPU(park1.Angle);
	park1.Cosine = _IQcosPU(park1.Angle);
	
	PARK_MACRO(park1) 

// ------------------------------------------------------------------------------
//    Connect inputs of the PID_GRANDO_CONTROLLER module and call the PID speed controller macro
// ------------------------------------------------------------------------------  
   if (SpeedLoopCount==SpeedLoopPrescaler)
     {
      pid1_spd.term.Ref = rc1.SetpointValue;
      pid1_spd.term.Fbk = speed3.EstimatedSpeed;
	  PID_GR_MACRO(pid1_spd)
      SpeedLoopCount=1;
     }
	else SpeedLoopCount++;   

	if(lsw==0 || lsw==1)
	{
		pid1_spd.data.ui=0;
		pid1_spd.data.i1=0;
	}

// ------------------------------------------------------------------------------
//    Connect inputs of the PID_GRANDO_CONTROLLER module and call the PID IQ controller macro
// ------------------------------------------------------------------------------  
	if(lsw==0) pid1_iq.term.Ref = 0;
    else if(lsw==1) pid1_iq.term.Ref = IqRef;
    else pid1_iq.term.Ref =  pid1_spd.term.Out; 
	pid1_iq.term.Fbk = park1.Qs;
	PID_GR_MACRO(pid1_iq)

// ------------------------------------------------------------------------------
//    Connect inputs of the PID_GRANDO_CONTROLLER module and call the PID ID controller macro
// ------------------------------------------------------------------------------  
	if(lsw==0) pid1_id.term.Ref = _IQ(0.05);
    else pid1_id.term.Ref = 0; 
	pid1_id.term.Fbk = park1.Ds;
	PID_GR_MACRO(pid1_id)

// ------------------------------------------------------------------------------
//  Connect inputs of the INV_PARK module and call the inverse park trans. macro
// ------------------------------------------------------------------------------   
    ipark1.Ds = pid1_id.term.Out;
    ipark1.Qs = pid1_iq.term.Out;
	ipark1.Sine=park1.Sine;
    ipark1.Cosine=park1.Cosine;
	IPARK_MACRO(ipark1) 

// ------------------------------------------------------------------------------
//    Connect inputs of the VOLT_CALC module and call the phase voltage macro
// ------------------------------------------------------------------------------
	#ifdef F2806x_DEVICE_H
	volt1.DcBusVolt = ((AdcResult.ADCRESULT2)*0.00024414); // DC Bus voltage meas.
    #endif														 // (ADCmeas(q12)/2^12)	
    
    #ifdef DSP2803x_DEVICE_H
	volt1.DcBusVolt = _IQ15toIQ((AdcResult.ADCRESULT3<<3));	     // DC Bus voltage meas.
    #endif

    volt1.MfuncV1 = svgen_dq1.Ta;
    volt1.MfuncV2 = svgen_dq1.Tb;
    volt1.MfuncV3 = svgen_dq1.Tc;        
	VOLT_MACRO(volt1)

// ------------------------------------------------------------------------------
//    Connect inputs of the SMO_POS module and call the sliding-mode observer macro
// ------------------------------------------------------------------------------
 	smo1.Ialpha = clarke1.Alpha;
  	smo1.Ibeta  = clarke1.Beta;
    smo1.Valpha = volt1.Valpha;
    smo1.Vbeta  = volt1.Vbeta;
	SMO_MACRO(smo1)	

// ------------------------------------------------------------------------------
//    Connect inputs of the SPEED_EST module and call the estimated speed macro
// ------------------------------------------------------------------------------
    speed3.EstimatedTheta = smo1.Theta;
	SE_MACRO(speed3)  

// ------------------------------------------------------------------------------
//  Connect inputs of the SVGEN_DQ module and call the space-vector gen. macro
// ------------------------------------------------------------------------------
  	svgen_dq1.Ualpha = ipark1.Alpha;
 	svgen_dq1.Ubeta = ipark1.Beta;
  	SVGEN_MACRO(svgen_dq1)        

// ------------------------------------------------------------------------------
//  Connect inputs of the PWM_DRV module and call the PWM signal generation macro
// ------------------------------------------------------------------------------
    pwm1.MfuncC1 = _IQtoQ15(svgen_dq1.Ta);
    pwm1.MfuncC2 = _IQtoQ15(svgen_dq1.Tb);  
    pwm1.MfuncC3 = _IQtoQ15(svgen_dq1.Tc);
	PWM_MACRO(pwm1)							   	   // Calculate the new PWM compare values	

	EPwm1Regs.CMPA.half.CMPA=pwm1.PWM1out;	// PWM 1A - PhaseA 
	EPwm2Regs.CMPA.half.CMPA=pwm1.PWM2out;	// PWM 2A - PhaseB
	EPwm3Regs.CMPA.half.CMPA=pwm1.PWM3out;	// PWM 3A - PhaseC  


// ------------------------------------------------------------------------------
//    Connect inputs of the PWMDAC module 
// ------------------------------------------------------------------------------	
    PwmDacCh1 = _IQtoQ15(clarke1.As);
    PwmDacCh2 = _IQtoQ15(clarke1.Bs); 
    PwmDacCh3 = _IQtoQ15(volt1.VphaseA);
    PwmDacCh4 = _IQtoQ15(smo1.Theta); 

// ------------------------------------------------------------------------------
//    Connect inputs of the DATALOG module 
// ------------------------------------------------------------------------------
    DlogCh1 = _IQtoQ15(clarke1.As);
    DlogCh2 = _IQtoQ15(clarke1.Bs);
    DlogCh3 = _IQtoQ15(volt1.VphaseA);
    DlogCh4 = _IQtoQ15(smo1.Theta);  

#endif // (BUILDLEVEL==LEVEL7) 
	}//end if(RunMotor)

// ------------------------------------------------------------------------------
//    Call the PWMDAC update macro.
// ------------------------------------------------------------------------------
	PWMDAC_MACRO(pwmdac1)

// ------------------------------------------------------------------------------
//    Call the DATALOG update function.
// ------------------------------------------------------------------------------
    dlog.update(&dlog);


#if (DSP2803x_DEVICE_H==1)||(DSP280x_DEVICE_H==1)||(F2806x_DEVICE_H==1)
// Enable more interrupts from this timer
	EPwm1Regs.ETCLR.bit.INT = 1;

// Acknowledge interrupt to recieve more interrupts from PIE group 3
	PieCtrlRegs.PIEACK.all = PIEACK_GROUP3;
#endif

}


interrupt void sciaRxFifoIsr(void)
{
    uint16_t datagram[16];

    datagram[0] = SciaRegs.SCIRXBUF.all;
    datagram[1] = SciaRegs.SCIRXBUF.all;
    datagram[2] = SciaRegs.SCIRXBUF.all;
    datagram[3] = SciaRegs.SCIRXBUF.all;

//    printf("what is rev %x %x\n",datagram[0],datagram[1]);

//    GpioDataRegs.GPBSET.bit.GPIO34 = 1;    // Blink LED

    GpioDataRegs.GPBTOGGLE.bit.GPIO34 = 1;    // Blink LED
    SciaRegs.SCIFFRX.bit.RXFFOVRCLR=1;   // Clear Overflow flag
    SciaRegs.SCIFFRX.bit.RXFFINTCLR=1;   // Clear Interrupt flag

    PieCtrlRegs.PIEACK.all|=0x100;       // Issue PIE ack

}


interrupt void sciaTxFifoIsr(void)
{

    SciaRegs.SCITXBUF = 0xaa;
    SciaRegs.SCITXBUF = 0x55;

    SciaRegs.SCIFFTX.bit.TXFFINTCLR=1;  // Clear SCI Interrupt flag
    PieCtrlRegs.PIEACK.all|=0x100;      // Issue PIE ACK

}


interrupt void timer0_100ms_isr(void)
{

    printf("i m in 100ms isr now \n");


    CpuTimer0Regs.TCR.bit.TIF = 1;

    PieCtrlRegs.PIEACK.all|= 0x001;   // Issue PIE ACK  cputimer1 and cputimer2 may trigger is isr if you do not add this line


}

interrupt void scibRxFifoIsr(void)
{
    ScibRegs.SCIFFRX.bit.RXFFOVRCLR=1;   // Clear Overflow flag
    ScibRegs.SCIFFRX.bit.RXFFINTCLR=1;   // Clear Interrupt flag

    uint16_t datagram[16];

    datagram[0] = ScibRegs.SCIRXBUF.all;
    datagram[1] = ScibRegs.SCIRXBUF.all;
    datagram[2] = ScibRegs.SCIRXBUF.all;
    datagram[3] = ScibRegs.SCIRXBUF.all;
    PieCtrlRegs.PIEACK.all|=0x100;       // Issue PIE ack
}


interrupt void scibTxFifoIsr(void)
{


}

void user_adc_init(void)
{
    AdcRegs.ADCCTL1.all=ADC_RESET_FLAG;
    asm(" NOP ");
    asm(" NOP ");

    EALLOW;
     AdcRegs.ADCCTL1.bit.ADCBGPWD   = 1;    /* Power up band gap */

    DELAY_US(ADC_usDELAY);                  /* Delay before powering up rest of ADC */

    AdcRegs.ADCCTL1.bit.ADCREFSEL   = 0;
    AdcRegs.ADCCTL1.bit.ADCREFPWD   = 1;    /* Power up reference */
    AdcRegs.ADCCTL1.bit.ADCPWDN     = 1;    /* Power up rest of ADC */
    AdcRegs.ADCCTL1.bit.ADCENABLE   = 1;    /* Enable ADC */

    asm(" RPT#100 || NOP");

    AdcRegs.ADCCTL1.bit.INTPULSEPOS=1;
    AdcRegs.ADCCTL1.bit.TEMPCONV=0;

    DELAY_US(ADC_usDELAY);

    AdcRegs.ADCCTL1.bit.TEMPCONV    = 1;    //Connect internal temp sensor to channel ADCINA5.

    AdcRegs.ADCSOC0CTL.bit.CHSEL    = 5;    //set SOC0 channel select to ADCINA5 (which is internally connected to the temperature sensor)
    AdcRegs.ADCSOC0CTL.bit.TRIGSEL  = 5;    //set SOC0 start trigger on EPWM1A
    AdcRegs.ADCSOC0CTL.bit.ACQPS    = 6;   //set SOC0 S/H Window to 26 ADC Clock Cycles, (25 ACQPS plus 1)
//    AdcRegs.INTSEL1N2.bit.INT1E     = 1;

    AdcRegs.ADCSOC1CTL.bit.CHSEL    = 0;    //ADC for ADCINA0 connected to the I_A1
    AdcRegs.ADCSOC1CTL.bit.TRIGSEL  = 5;
    AdcRegs.ADCSOC1CTL.bit.ACQPS    = 6;

    AdcRegs.ADCSOC2CTL.bit.CHSEL    = 8;    //ADC for ADCINB0 connected to the I_B1
    AdcRegs.ADCSOC2CTL.bit.TRIGSEL  = 5;
    AdcRegs.ADCSOC2CTL.bit.ACQPS    = 6;

    AdcRegs.ADCSOC3CTL.bit.CHSEL    = 1;    //ADC for ADCINA1 connected to the I_C1
    AdcRegs.ADCSOC3CTL.bit.TRIGSEL  = 5;
    AdcRegs.ADCSOC3CTL.bit.ACQPS    = 6;

    AdcRegs.ADCSOC4CTL.bit.CHSEL    = 9;    //ADC for ADCINB1 connected to the V_A1
    AdcRegs.ADCSOC4CTL.bit.TRIGSEL  = 5;
    AdcRegs.ADCSOC4CTL.bit.ACQPS    = 6;

    AdcRegs.ADCSOC5CTL.bit.CHSEL    = 2;    //ADC for ADCINA2 connected to the V_B1
    AdcRegs.ADCSOC5CTL.bit.TRIGSEL  = 5;
    AdcRegs.ADCSOC5CTL.bit.ACQPS    = 6;

    AdcRegs.ADCSOC6CTL.bit.CHSEL    = 10;   //ADC for ADCINB2 connected to the V_C1
    AdcRegs.ADCSOC6CTL.bit.TRIGSEL  = 5;
    AdcRegs.ADCSOC6CTL.bit.ACQPS    = 6;

    AdcRegs.ADCSOC7CTL.bit.CHSEL    = 7;    //ADC for ADCINA7 connected to the V_DC
    AdcRegs.ADCSOC7CTL.bit.TRIGSEL  = 5;
    AdcRegs.ADCSOC7CTL.bit.ACQPS    = 6;

    EDIS;



}

void user_qep_init(void)
{
    // Initialize QEP module
    qep1.LineEncoder = 1000;
    qep1.MechScaler = _IQ30(0.25/qep1.LineEncoder);
    qep1.PolePairs = POLES/2;
    qep1.CalibratedAngle = 0;
    QEP_INIT_MACRO(qep1)

}


void user_epwm_init(void)
{
    /* Set up Event Trigger with CNT_zero enable for Time-base of EPWM1 */
    EPwm1Regs.ETSEL.bit.SOCAEN = 1;     /* Enable SOCA */
    EPwm1Regs.ETSEL.bit.SOCASEL = 1;    /* Enable CNT_zero event for SOCA */
    EPwm1Regs.ETPS.bit.SOCAPRD = 1;     /* Generate SOCA on the 1st event */
    EPwm1Regs.ETCLR.bit.SOCA = 1;       /* Clear SOCA flag */
//    EPwm1Regs.CMPA.half.CMPA    = 0x0080;   // Set compare A value
//    EPwm1Regs.TBPRD              = SYSTEM_FREQUENCY*1000000*T/2;;   // Set period for ePWM1
    EPwm1Regs.TBCTL.bit.CTRMODE  = 0;        // count up and start
}

void user_pwm_init(void)
{
    // Initialize PWM module
    pwm1.PeriodMax = SYSTEM_FREQUENCY*1000000*T/2;  // Prescaler X1 (T1), ISR period = T x 1
    PWM_INIT_MACRO(pwm1)


    // Initialize PWMDAC module
    pwmdac1.PeriodMax = 500;   // @60Mhz: 1500->20kHz, 1000-> 30kHz, 500->60kHz
    pwmdac1.PwmDacInPointer0 = &PwmDacCh1;
    pwmdac1.PwmDacInPointer1 = &PwmDacCh2;
    pwmdac1.PwmDacInPointer2 = &PwmDacCh3;
    pwmdac1.PwmDacInPointer3 = &PwmDacCh4;

    PWMDAC_INIT_MACRO(pwmdac1)
}

void user_drv8301_init(void)
{
    DRV8301_SPI_Init(&SpiaRegs);
    GpioDataRegs.GPBCLEAR.bit.GPIO50 = 1;

//    EALLOW;
//    EPwm1Regs.TZFRC.bit.OST=1;
//    EPwm2Regs.TZFRC.bit.OST=1;
//    EPwm3Regs.TZFRC.bit.OST=1;
//    EDIS;

    DELAY_US(500000);

#ifdef F2806x_DEVICE_H
//    GpioDataRegs.GPBSET.bit.GPIO50 = 1;
    GpioDataRegs.GPBSET.bit.GPIO51 = 1;
#endif

    DELAY_US(50000);        //delay to allow DRV830x supplies to ramp up

    DRV8301_cntrl_reg1.bit.GATE_CURRENT = 0;        // full current 1.7A
//  DRV8301_cntrl_reg1.bit.GATE_CURRENT = 1;        // med current 0.7A
//  DRV8301_cntrl_reg1.bit.GATE_CURRENT = 2;        // min current 0.25A
    DRV8301_cntrl_reg1.bit.GATE_RESET = 0;          // Normal Mode
    DRV8301_cntrl_reg1.bit.PWM_MODE = 0;            // six independant PWMs
//  DRV8301_cntrl_reg1.bit.OC_MODE = 0;             // current limiting when OC detected
    DRV8301_cntrl_reg1.bit.OC_MODE = 1;             // latched OC shutdown
//  DRV8301_cntrl_reg1.bit.OC_MODE = 2;             // Report on OCTWn pin and SPI reg only, no shut-down
//  DRV8301_cntrl_reg1.bit.OC_MODE = 3;             // OC protection disabled
//  DRV8301_cntrl_reg1.bit.OC_ADJ_SET = 0;          // OC @ Vds=0.060V
//  DRV8301_cntrl_reg1.bit.OC_ADJ_SET = 4;          // OC @ Vds=0.097V
//  DRV8301_cntrl_reg1.bit.OC_ADJ_SET = 6;          // OC @ Vds=0.123V
//  DRV8301_cntrl_reg1.bit.OC_ADJ_SET = 9;          // OC @ Vds=0.175V
    DRV8301_cntrl_reg1.bit.OC_ADJ_SET = 15;         // OC @ Vds=0.358V
//  DRV8301_cntrl_reg1.bit.OC_ADJ_SET = 16;         // OC @ Vds=0.403V
//  DRV8301_cntrl_reg1.bit.OC_ADJ_SET = 17;         // OC @ Vds=0.454V
//  DRV8301_cntrl_reg1.bit.OC_ADJ_SET = 18;         // OC @ Vds=0.511V
    DRV8301_cntrl_reg1.bit.Reserved = 0;

//  DRV8301_cntrl_reg2.bit.OCTW_SET = 0;            // report OT and OC
    DRV8301_cntrl_reg2.bit.OCTW_SET = 1;            // report OT only
#if DRV_GAIN == 10
    DRV8301_cntrl_reg2.bit.GAIN = 0;                // CS amplifier gain = 10
#elif DRV_GAIN == 20
    DRV8301_cntrl_reg2.bit.GAIN = 1;                // CS amplifier gain = 20
#elif DRV_GAIN == 40
    DRV8301_cntrl_reg2.bit.GAIN = 2;                // CS amplifier gain = 40
#elif DRV_GAIN == 80
    DRV8301_cntrl_reg2.bit.GAIN = 3;                // CS amplifier gain = 80
#endif
    DRV8301_cntrl_reg2.bit.DC_CAL_CH1 = 0;          // not in CS calibrate mode
    DRV8301_cntrl_reg2.bit.DC_CAL_CH2 = 0;          // not in CS calibrate mode
    DRV8301_cntrl_reg2.bit.OC_TOFF = 0;             // normal mode
    DRV8301_cntrl_reg2.bit.Reserved = 0;

    //write to DRV8301 control register 1, returns status register 1
    DRV8301_stat_reg1.all = DRV8301_SPI_Write(&SpiaRegs,CNTRL_REG_1_ADDR,DRV8301_cntrl_reg1.all);
    //write to DRV8301 control register 2, returns status register 1
    DRV8301_stat_reg1.all = DRV8301_SPI_Write(&SpiaRegs,CNTRL_REG_2_ADDR,DRV8301_cntrl_reg2.all);

}

//===========================================================================
// No more.
//===========================================================================
