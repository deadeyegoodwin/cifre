# Connect4

Two player game played on 6x7 board. Players take turn dropping pieces
into any non-full column. The first player to get 4 in a row, any
direction, is the winner. Other board sizes can be trained and played
using the --game-shape flag to cifre_train.py and connect4
executables. For example, use --game-shape=8,9 to use a 8x9 board.

The following trains a network to play Connect4. `NOTE` that the
following first removes models and training results from
/tmp/cifre/models, so update appropriately if you don't want that data
deleted.

```bash
rm -fr /tmp/cifre/models/* && rm -fr /tmp/cifre/train/* && ./cifre_train.py --repo-dir=/tmp/cifre/models --train-dir=/tmp/cifre/train --game=connect4 --epoch-count=20 --iteration-count=8 --game-count=100
```

Default settings for alpha-zero can be found in main.cc
Connect4::DefaultAlphaZeroSettings.

After training a model you can play individual games against alphazero
using that model. Assuming the best trained model is called 'iter3':

```bash
install/bin/connect4 --player1=human --player2=alphazero --alphazero-model iter3
```

To have player2 go first add the --starting argument after --player2.

```bash
install/bin/connect4 --player1=human --player2=alphazero --starting --alphazero-model iter3
```

You can also have a random or alphazero player that plays against a
human or each other. For example, use the following command to play a
random player against the alphazero model. Use --game-count to play
multiple games.

```bash
install/bin/connect4 --game-count=10 --player1=random --player2=alphazero --alphazero-model iter3
```

You can even play two alphazero models against each other.

```bash
install/bin/connect4 --game-count=10 --player1=alphazero --alphazero-model <another model name> --player2=alphazero --alphazero-model iter3
```
