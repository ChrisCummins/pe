# List of demonstrations
demos=(snow fountains fireworks catherine_wheel galaxy)

# Time to run each demonstration for
time=30

(
	# Run in sub-shell to suppress stderr garbage from kill
	for d in ${demos[*]}; do
		echo "./$d for $time seconds..."
		(cmdpid=$BASHPID; (sleep $time; kill $cmdpid) & exec ./$d)
	done

	echo "Demonstrations complete"
) 2>/dev/null
