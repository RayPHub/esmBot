#include <napi.h>

#include <vips/vips8>

using namespace std;
using namespace vips;

Napi::Value Whisper(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  try {
    Napi::Object obj = info[0].As<Napi::Object>();
    Napi::Buffer<char> data = obj.Get("data").As<Napi::Buffer<char>>();
    string caption = obj.Get("caption").As<Napi::String>().Utf8Value();
    string type = obj.Get("type").As<Napi::String>().Utf8Value();

    VOption *options = VImage::option()->set("access", "sequential");

    VImage in =
        VImage::new_from_buffer(data.Data(), data.Length(), "",
                                type == "gif" ? options->set("n", -1) : options)
            .colourspace(VIPS_INTERPRETATION_sRGB);
    if (!in.has_alpha()) in = in.bandjoin(255);

    int width = in.width();
    int page_height = vips_image_get_page_height(in.get_image());
    int n_pages = vips_image_get_n_pages(in.get_image());
    int size = width / 6;
    int dividedWidth = width / 175;
    int rad = 1;

    string font_string = "Upright " + to_string(size);

    VImage mask;
    if (dividedWidth >= 1) {
      mask = VImage::black(dividedWidth * 2 + 1, dividedWidth * 2 + 1) + 128;
      mask.draw_circle({255}, dividedWidth, dividedWidth, dividedWidth,
                       VImage::option()->set("fill", true));
    } else {
      mask = VImage::black(rad * 2 + 1, rad * 2 + 1) + 128;
      mask.draw_circle({255}, rad, rad, rad,
                       VImage::option()->set("fill", true));
    }

    VImage textIn = VImage::text(
        ("<span foreground=\"white\">" + caption + "</span>").c_str(),
        VImage::option()
            ->set("rgba", true)
            ->set("align", VIPS_ALIGN_CENTRE)
            ->set("font", font_string.c_str())
            ->set("width", width));

    textIn = textIn.embed(rad + 10, rad + 10, (textIn.width() + 2 * rad) + 20,
                          (textIn.height() + 2 * rad) + 20);

    VImage outline =
        textIn.morph(mask, VIPS_OPERATION_MORPHOLOGY_DILATE)
            .gaussblur(0.5, VImage::option()->set("min_ampl", 0.1));
    outline = (outline == (vector<double>){0, 0, 0, 0});
    VImage invert = outline.extract_band(3).invert();
    outline =
        outline.extract_band(0, VImage::option()->set("n", outline.bands() - 1))
            .bandjoin(invert);
    VImage textImg = outline.composite2(textIn, VIPS_BLEND_MODE_OVER);

    vector<VImage> img;
    for (int i = 0; i < n_pages; i++) {
      VImage img_frame =
          type == "gif" ? in.crop(0, i * page_height, width, page_height) : in;
      img_frame = img_frame.composite2(
          textImg, VIPS_BLEND_MODE_OVER,
          VImage::option()
              ->set("x", (width / 2) - (textImg.width() / 2))
              ->set("y", (page_height / 2) - (textImg.height() / 2)));
      img.push_back(img_frame);
    }
    VImage final = VImage::arrayjoin(img, VImage::option()->set("across", 1));
    final.set(VIPS_META_PAGE_HEIGHT, page_height);

    void *buf;
    size_t length;
    final.write_to_buffer(
        ("." + type).c_str(), &buf, &length,
        type == "gif" ? VImage::option()->set("dither", 0) : 0);

    vips_thread_shutdown();

    Napi::Object result = Napi::Object::New(env);
    result.Set("data", Napi::Buffer<char>::Copy(env, (char *)buf, length));
    result.Set("type", type);
    return result;
  } catch (std::exception const &err) {
    throw Napi::Error::New(env, err.what());
  } catch (...) {
    throw Napi::Error::New(env, "Unknown error");
  }
}