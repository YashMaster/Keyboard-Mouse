import sys

chars = {}

#Frequently used
chars["SHIFT"] = 				"0b1000000000"	
chars["\t"] = 					"0b0100000000"
chars[" "] = 					"0b0010000000"
chars["\n"] = 					"0b0001000000"
chars["SPECIAL_ACTIVATOR"] = 	"0b0000100000"
chars["ALT"] = 					"0b0000010000"
# Left and right click???
chars["BACK_SPACE"] = 			"0b0000000010"
chars["CTRL"] = 				"0b0000000001"


#Regular letters
chars["e"] = 					"0b0001001000"
chars["t"] =					"0b0000001100"
chars["a"] = 					"0b0000000110"
chars["o"] = 					"0b0001000100"
chars["i"] =					"0b0010001000"
chars["n"] = 					"0b0010000100"
chars["s"] = 					"0b0100000010"
chars["h"] = 					"0b0101000000"
chars["r"] = 					"0b0000001010"
chars["d"] = 					"0b0011000000"
chars["l"] = 					"0b0001000010"
chars["c"] = 					"0b0110000000"
chars["u"] = 					"0b0100001000"
chars["m"] =					"0b0010000010"
chars["w"] = 					"0b0100000100"
chars["f"] = 					"0b0001001100"
chars["g"] = 					"0b0011001000"
chars["y"] = 					"0b0011000100"
chars["p"] = 					"0b0010001100"
chars["b"] = 					"0b0011000010"
chars["v"] =					"0b0100001100"
chars["k"] =					"0b0110001000"
chars["j"] = 					"0b0001000110"
chars["x"] = 					"0b0110000100"
chars["q"] = 					"0b0010000110"
chars["z"] =					"0b0101001000"
# Special case, shift+alt+ctrl == function button, maybe thing of better
# way to do this

# Special Characters (must be used with left thumb down)
chars["."] = 					"0b0000101000"
chars[","] = 					"0b0000100100"
chars[">"] = 					"0b1000101000"
chars["<"] = 					"0b0000100100"

# Colon is shifted version
chars[";"] =					"0b0000100010"
chars["{"] = 					"0b0100100000"
chars["}"] = 					"0b0010100000"
chars["\'"] = 					"0b0000100110"
chars["/"] =					"0b0110100000"

# Numbers
chars["0"] = 					"0b0111100000"
chars["1"] = 					"0b0110101000"
chars["2"] =					"0b0110100100"
chars["3"] = 					"0b0110100010"
chars["4"] = 					"0b0011101000"
chars["5"] =					"0b0011100100"
chars["6"] = 					"0b0011100010"
chars["7"] =					"0b0001101100"
chars["8"] = 					"0b0001101010"
chars["9"] = 					"0b0000101110"


def main():
	input_string = sys.argv[1]
	for word in input_string.split():
		word_func(word)


def word_func(word):
	print word
	for character in word:
		character = character.lower()
		if character in chars:
			print character + ": " + chars[character][2:6] + " " + chars[character][8:12]
	word_in = raw_input()
	if word_in != word:
		print "Incorrect. Try again."
		word_func(word)

if __name__ == '__main__':
	main()