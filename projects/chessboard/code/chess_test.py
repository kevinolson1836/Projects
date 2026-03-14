''' 

notes:
    
    print(board)
        PRINTS BOARD IN HUMAN READABLE FORMAT
    
    board.push(MOVE)
        PUSHES THE MOVE ONTO THE REAL BOARD 
        
    chess.Move.from_uci("g1f3")
        MOVES PEICE ON SQUARE G1 TO SQUARE F3
        
    board.is_check()
        checks for check. only returns True or False
        
        
'''


import chess

board = chess.Board()

print(board.legal_moves)


print()

legal_moves = list(board.legal_moves)
print(legal_moves)

print()
print()

# print(board)

# board.push(chess.Move.from_uci("g1f3"))


print()
print()
print()

print(board)

# idea label pins on chess board 0-63. detect what pin's state changed and get that number and convert it into its chess square
# update the LEDS on the chess board with 
print(chess.square_name(4))



def legal_moves_for_peice(board, peice):
    legal_moves = list(board.legal_moves)
    startLocation = peice # a 2 char string that represents the first location to move from
    possibleMoves = []
    possibleMovesLocations = []
    endLocation = 0
    print("start location: " + startLocation)
    print()
    print(board)
    print()
    
    for move in legal_moves:
        move = str(move)
        # print("checking: " + str(move)[0:2])
        if (move[0:2] == startLocation):
            print("legal move: " + move)
            possibleMoves.append(move)
            possibleMovesLocations.append(move[2:4])
            
    print()
    print(possibleMoves)
    print(possibleMovesLocations)
    
    
    
print()
print()
print()
legal_moves_for_peice(board, "b1")
print()
print()
