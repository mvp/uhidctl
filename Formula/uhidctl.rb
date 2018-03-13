class Uhidctl < Formula
  desc "Control USB HID power relays"
  homepage "https://github.com/mvp/uhidctl"
  head "https://github.com/mvp/uhidctl.git"

  depends_on "hidapi"
  depends_on "pkg-config" => :build

  def install
    system "make"
    bin.install "uhidctl"
  end

  test do
    system "#{bin}/uhidctl", "-v"
  end
end
