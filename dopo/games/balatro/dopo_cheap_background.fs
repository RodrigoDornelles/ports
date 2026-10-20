// Cheaper background
//
// The animated background shader runs 2 iterations instead of 5.

//! @patch resources/shaders/background.fs 36-36
    for(int i=0; i < 2; i++) {
//! @endpatch
