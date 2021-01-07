import PySimpleGUI as sg
import socket
from time import sleep


def handle_login(s):

    layout = [
        [sg.Text('Enter username and password.')],
        [sg.Text('Username:', size=(15, 1)), sg.InputText()],
        [sg.Text('Password:', size=(15, 1)), sg.InputText(password_char='*')],
        [sg.Submit(), sg.Cancel()]
    ]
    window = sg.Window('SK2_Komunikator', layout)

    while True:

        event, values = window.read()

        s.send(list(values.values())[0].encode())
        sleep(1)
        s.send(list(values.values())[1].encode())

        data = s.recv(1024).decode().strip()
        sg.popup(data)

        if data[6] == "s":
            window.close()
            return list(values.values())[0]


def logout(s):
    s.send("l".encode())
    return


def get_friends(s):
    s.send("f".encode())
    data = s.recv(1024).decode().split(",")
    return data


def get_msgs(s):
    s.send("m".encode())
    number = s.recv(16)
    msgs = []
    for _ in range(number):
        msgs.append(s.recv(1024).decode())
    return msgs


def invite(s):

    layout = [
        [sg.Text('Add friend')],
        [sg.Text('Username:', size=(15, 1)), sg.InputText()],
        [sg.Button('Submit'), sg.Button('Cancel')]
    ]
    window = sg.Window('Adding friend', layout)
    username = ""
    while True:

        event, values = window.read()

        if event == 'Cancel':
            return

        if event == 'Submit':
            username = list(values.values())[0]
            window.close()
            break

    s.send("a".encode())
    sleep(1)
    s.send(username.encode())
    data = s.recv(1024).decode()
    sg.popup(data)
    return


def send_msg(s, to, content):
    s.send("m".encode())
    sleep(1)
    s.send(to.encode())
    sleep(1)
    s.send(content.encode())


def main():

    # Connection start
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('192.168.1.64', 1124))

    # Login phase
    username = handle_login(s)

    # Preparing the main window
    friends = get_friends(s)
    messages = get_msgs(s)

    layout = [
        [sg.Listbox(values=friends, size=(20, 12), key='friends', enable_events=True),
         sg.Column([[sg.Txt('64-RD  06.01.2021, 13:42\nKledzik chuj', size=(50,20), key='msgs', background_color='grey')],
                    [sg.Input(key='input', size=(20,5)), sg.Button('Send')]])],
        [sg.Button('Add friend'), sg.Button('Logout')]
    ]

    # The main GUI part.
    window = sg.Window('SK2_Komunikator', layout)

    while True:

        event, values = window.read()

        if event == 'Logout' or event == sg.WIN_CLOSED:
            logout(s)
            break

        if event == 'Send' and values['input'] != '':
            send_msg(s, values['friends'], values['input'])

        if event == 'Add friend':
            invite(s)

    window.close()


if __name__ == "__main__":
    main()
