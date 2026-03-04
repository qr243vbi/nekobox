#!/bin/bash
# 1. Collect target users (Followers + Stargazers)
gh api --paginate users/qr243vbi/followers --jq '.[].login' > targets.txt
gh api --paginate repos/qr243vbi/nekobox/stargazers --jq '.[].login' >> targets.txt
sort -u targets.txt -o targets.txt

# 2. Get your current following list
# Replace 'YOUR_USERNAME' with your actual GitHub handle
gh api --paginate users/qr243vbi/following --jq '.[].login' | sort > currently_following.txt

# 3. Follow ONLY those NOT already followed
# (Finds lines in targets.txt that are NOT in currently_following.txt)
comm -23 targets.txt currently_following.txt > to_follow.txt
while read user; do
  echo "Following new user: $user"
  gh api -X PUT "user/following/$user" | cat
done < to_follow.txt

# 4. Unfollow everyone else
# (Finds lines in currently_following.txt that are NOT in targets.txt)
comm -13 targets.txt currently_following.txt > to_unfollow.txt
while read user; do
  echo "Unfollowing: $user"
  gh api -X DELETE "user/following/$user" | cat
done < to_unfollow.txt
