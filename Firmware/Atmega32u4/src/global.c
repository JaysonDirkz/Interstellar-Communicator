#ifndef global_h
#define global_h

int8_t cv_pins[4];
int8_t gate_pins[4];

int8_t clockCounter = -1;
int8_t resetCounter = -1;
int8_t trigCounter[4] = {-1, -1, -1, -1};

//Functions.
typedef void WriteFunction_t(KeysBase *); // For declaring function pointers.




// enum InPin: in8_t {
//     Button1 = A0,
//     Button2 = A1,
//     Button3 = A2,
//     Button4 = A3
// }

// enum OutPin: int8_t {
//     Clock = 2,
//     Reset = 3,
//     Gate1 = 4,
//     Gate2 = 5,
//     Gate3 = 7,
//     Gate4 = 8
// };

// const int8_t rowAmount = 4;
// const int8_t buttonsAmount = rowAmount;
// const int8_t gateOutsAmount = rowAmount;
// const int8_t cvOutsAmount = rowAmount;
#endif