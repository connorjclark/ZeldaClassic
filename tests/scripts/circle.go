// https://tinygo.org/getting-started/install/
// tinygo build -o circle.wasm -target wasm circle.go

// TODO: use a fixed int type

package main

import (
	"math"
)

//export get_register
func get_register(arg int) int

//export set_register
func set_register(arg, value int)

// TODO need to refactor all of run_script_int
// func run_command(command, arg1, arg2 int)
// ... for now, hardcode a run_command_rnd
//export run_command_rnd
func run_command_rnd(arg int) int

const FX = 0x000D
const FY = 0x000E

func setX(value float32) {
	set_register(FX, int(value*10000))
}

func setY(value float32) {
	set_register(FY, int(value*10000))
}

func cos(degrees float32) float32 {
	rads := float64(degrees * math.Pi / 180)
	return float32(math.Cos(rads))
}

func sin(degrees float32) float32 {
	rads := float64(degrees * math.Pi / 180)
	return float32(math.Sin(rads))
}

var radius, speed, angle, radius2, angle2 float32
var cx, cy float32

//export initialize
func initialize(radius_, speed_, angle_, radius2_, angle2_ int) {
	radius = float32(radius_) / 10000.0
	speed = float32(speed_) / 10000.0
	angle = float32(angle_) / 10000.0
	radius2 = float32(radius2_) / 10000.0
	angle2 = float32(angle2_) / 10000.0

	if radius2 == 0 {
		radius2 = radius
	}
	if angle < 0 {
		// TODO: using `time` requires providing `gojs/runtime.ticks` host function.
		// rand.Seed(time.Now().UnixNano())
		// angle = rand.Float32() * 360
		angle = float32(run_command_rnd(360))
	}

	cx = float32(get_register(FX)) / 10000.0
	cy = float32(get_register(FY)) / 10000.0
}

//export run
func run() int {
	angle += speed

	if angle < -360 {
		// Wrap if below -360.
		angle += 360
	} else if angle > 360 {
		// Wrap if above 360.
		angle -= 360
	}

	if angle2 == 0 {
		setX(cx + radius*cos(angle))
		setY(cy + radius2*sin(angle))
	} else {
		// Rotate at center.
		setX(cx + radius*cos(angle)*cos(angle2) - radius2*sin(angle)*sin(angle2))
		setY(cy + radius2*sin(angle)*cos(angle2) + radius*cos(angle)*sin(angle2))
	}

	return 0
}

func main() {}
