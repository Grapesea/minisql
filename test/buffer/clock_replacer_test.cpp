#include "buffer/clock_replacer.h"

#include "gtest/gtest.h"

TEST(CLOCKReplacerTest, EmptyReplacer) {
  CLOCKReplacer replacer(3);
  frame_id_t victim = -1;

  EXPECT_EQ(0U, replacer.Size());
  EXPECT_FALSE(replacer.Victim(&victim));
}

TEST(CLOCKReplacerTest, DuplicateUnpinDoesNotIncreaseSize) {
  CLOCKReplacer replacer(4);

  replacer.Unpin(1);
  replacer.Unpin(1);
  replacer.Unpin(1);

  EXPECT_EQ(1U, replacer.Size());
}

TEST(CLOCKReplacerTest, PinRemovesCandidate) {
  CLOCKReplacer replacer(4);

  replacer.Unpin(0);
  replacer.Unpin(1);
  replacer.Unpin(2);
  EXPECT_EQ(3U, replacer.Size());

  replacer.Pin(1);
  EXPECT_EQ(2U, replacer.Size());

  frame_id_t victim = -1;
  ASSERT_TRUE(replacer.Victim(&victim));
  EXPECT_EQ(0, victim);
  ASSERT_TRUE(replacer.Victim(&victim));
  EXPECT_EQ(2, victim);
  EXPECT_FALSE(replacer.Victim(&victim));
}

TEST(CLOCKReplacerTest, SecondChanceScanOrder) {
  CLOCKReplacer replacer(5);

  replacer.Unpin(0);
  replacer.Unpin(1);
  replacer.Unpin(2);

  frame_id_t victim = -1;
  ASSERT_TRUE(replacer.Victim(&victim));
  EXPECT_EQ(0, victim);
  ASSERT_TRUE(replacer.Victim(&victim));
  EXPECT_EQ(1, victim);
  ASSERT_TRUE(replacer.Victim(&victim));
  EXPECT_EQ(2, victim);
  EXPECT_FALSE(replacer.Victim(&victim));
}

TEST(CLOCKReplacerTest, InvalidFrameIdIgnored) {
  CLOCKReplacer replacer(3);

  replacer.Unpin(-1);
  replacer.Unpin(3);
  EXPECT_EQ(0U, replacer.Size());

  replacer.Unpin(1);
  EXPECT_EQ(1U, replacer.Size());

  replacer.Pin(-2);
  replacer.Pin(5);
  EXPECT_EQ(1U, replacer.Size());

  frame_id_t victim = -1;
  ASSERT_TRUE(replacer.Victim(&victim));
  EXPECT_EQ(1, victim);
  EXPECT_FALSE(replacer.Victim(&victim));
}
