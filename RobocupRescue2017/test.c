#pragma config(Motor,  motorA,          L,             tmotorEV3_Large, PIDControl, encoder)
#pragma config(Motor,  motorD,          R,             tmotorEV3_Large, PIDControl, encoder)
//*!!Code automatically generated by 'ROBOTC' configuration wizard               !!*//

task main()
{
while(1){
motor[L]=100;
motor[R]=100;
}
}
