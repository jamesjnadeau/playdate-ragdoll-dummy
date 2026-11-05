# playdate-ragdoll-dummy

This is a game for the playdate console. You walk down an endless hallway with doors. 
Out of these doors spawns punching bag dummies with arms. They should be of 3 different sizes.
They should look like cartoon characters with menacing faces. They should always path towards the player.
The user picks up various melee weapons off the ground as they go through the hallway.
The player cranks to swing their weapon to defend themselves. 
The direction on the dpad held determines the swing direction. So holding left has the swing come from the left, up or down is the default swing. 
Use the up and down dpad keys to aim the camera and hit direction up or down. 
The crank direction determines if they swing up or down. 
Enemies have a small health pool, and depending on swing speed and hit location, they loose health. 
When their health is gone, they are ragdolled away like a cartoon. When they hit a hard service and loose their ragdoll health pool, the bust open and their filling falls out, and then they disappear. 
I'd like this game to be a 3d perspective, but keep the art assets 2d. Keep in mind I'd like these assets to have a spec so they can be easily added to by others.
I want the art to be modular, so folks can easily swap in their own work to make the game fresh and entertaining. 
The fun is hitting things and seeing how they react. There should be reactions to each of the directions someone hits from, and it should be funny and fun to play.
I'd like for this to be built in C and run on the playdate SDK.

## Building and running

Needs the Playdate SDK (`PLAYDATE_SDK_PATH`, default `~/Developer/PlaydateSDK-3.1.2`). Deploying also needs `arm-none-eabi-gcc` and `udisksctl`.

```
tools/sim.sh            # build RagdollDummies.pdx and open it in the simulator
tools/deploy.sh         # build for hardware, copy it to a USB-connected Playdate and launch it
make test               # host unit tests (no SDK needed)
```

`tools/deploy.sh` finds the Playdate's serial port, puts it in data-disk mode, copies the game into `Games/`, ejects it and starts the game. Pass `--port` to pick the port, `--no-run` to skip the launch, or `--keep-mounted` to leave the disk mounted. `tools/sim.sh --no-run` only builds.
