import ImageCommand from "../../classes/imageCommand.js";

class SooSCommand extends ImageCommand {
  params() {
    return {
      soos: true
    };
  }

  static description = "\"Loops\" an image sequence by reversing it when it's finished";
  static aliases = ["bounce", "boomerang"];

  static requiresGIF = true;
  static noImage = "You need to provide an image/GIF to loop!";
  static command = "reverse";
}

export default SooSCommand;
