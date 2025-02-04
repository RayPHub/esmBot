#include <napi.h>

#include <vips/vips8>

using namespace std;
using namespace vips;

Napi::Value Jpeg(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  try {
    Napi::Object obj = info[0].As<Napi::Object>();
    Napi::Buffer<char> data = obj.Get("data").As<Napi::Buffer<char>>();
    int quality = obj.Has("quality")
                      ? obj.Get("quality").As<Napi::Number>().Int32Value()
                      : 0;
    string type = obj.Get("type").As<Napi::String>().Utf8Value();

    Napi::Object result = Napi::Object::New(env);

    if (type == "gif") {
      VImage in =
          VImage::new_from_buffer(
              data.Data(), data.Length(), "",
              VImage::option()->set("access", "sequential")->set("n", -1))
              .colourspace(VIPS_INTERPRETATION_sRGB);
      if (!in.has_alpha()) in = in.bandjoin(255);

      int page_height = vips_image_get_page_height(in.get_image());

      void *jpgBuf;
      size_t jpgLength;
      in.write_to_buffer(
          ".jpg", &jpgBuf, &jpgLength,
          VImage::option()->set("Q", quality)->set("strip", true));
      VImage final = VImage::new_from_buffer(jpgBuf, jpgLength, "");
      final.set(VIPS_META_PAGE_HEIGHT, page_height);
      if (type == "gif") final.set("delay", in.get_array_int("delay"));

      void *buf;
      size_t length;
      final.write_to_buffer(
          ("." + type).c_str(), &buf, &length,
          type == "gif" ? VImage::option()->set("dither", 0) : 0);

      vips_thread_shutdown();

      result.Set("data", Napi::Buffer<char>::Copy(env, (char *)buf, length));
      result.Set("type", type);
    } else {
      VImage in = VImage::new_from_buffer(data.Data(), data.Length(), "");
      void *buf;
      size_t length;
      in.write_to_buffer(
          ".jpg", &buf, &length,
          VImage::option()->set("Q", quality)->set("strip", true));

      vips_thread_shutdown();

      result.Set("data", Napi::Buffer<char>::Copy(env, (char *)buf, length));
      result.Set("type", "jpg");
    }

    return result;
  } catch (std::exception const &err) {
    throw Napi::Error::New(env, err.what());
  } catch (...) {
    throw Napi::Error::New(env, "Unknown error");
  }
}
