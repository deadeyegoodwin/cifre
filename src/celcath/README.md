# Celcath

A two player cellular automata game played on 8x8 board. Other board
sizes can also be trained for and played on using the --game-shape
flag to cifre_train.py and celcath executables. For example, use
--game-shape=6,8 to use a 6x8 board.

The following trains a network to play Celcath. `NOTE` that the
following first removes models and training results from
/tmp/cifre/models, so update appropriately if you don't want that data
deleted.

```bash
rm -fr /tmp/cifre/models/* && rm -fr /tmp/cifre/train/* && ./cifre_train.py --repo-dir=/tmp/cifre/models --train-dir=/tmp/cifre/train --game=celcath --epoch-count=20 --iteration-count=8 --game-count=100
```

Default settings for alpha-zero can be found in main.cc
Celcath::DefaultAlphaZeroSettings.

After training a model you can play individual games against alphazero
using that model. Assuming the best trained model is called 'iter3':

```bash
install/bin/celcath --player1=human --player2=alphazero --alphazero-model iter3
```

To have player2 go first add the --starting argument after --player2.

```bash
install/bin/celcath --player1=human --player2=alphazero --starting --alphazero-model iter3
```

You can also have a random or alphazero player that plays against a
human or each other. For example, use the following command to play a
random player against the alphazero model. Use --game-count to play
multiple games. `NOTE` it is not recommended to use a *perfect* player
with Celcath as the computation time for perfect play is likely not
reasonable.

```bash
install/bin/celcath --game-count=10 --player1=random --player2=alphazero --alphazero-model iter3
```

You can even play two alphazero models against each other.

```bash
install/bin/celcath --game-count=10 --player1=alphazero --alphazero-model <another model name> --player2=alphazero --alphazero-model iter3
```
