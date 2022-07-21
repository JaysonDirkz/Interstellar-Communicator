#ifndef global_h
#define global_h

typedef int8_t i8;
typedef uint8_t u8;

i8 cv_pins[4];
i8 gate_pins[4];

i8 clockCounter = -1;
i8 resetCounter = -1;
i8 trigCounter[4] = {-1, -1, -1, -1};

//Functions.
typedef void WriteFunction_t(KeysBase *); // For declaring function pointers.




// enum InPin: in8_t {
//     Button1 = A0,
//     Button2 = A1,
//     Button3 = A2,
//     Button4 = A3
// }

// enum OutPin: i8 {
//     Clock = 2,
//     Reset = 3,
//     Gate1 = 4,
//     Gate2 = 5,
//     Gate3 = 7,
//     Gate4 = 8
// };

// const i8 rowAmount = 4;
// const i8 buttonsAmount = rowAmount;
// const i8 gateOutsAmount = rowAmount;
// const i8 cvOutsAmount = rowAmount;
#endif